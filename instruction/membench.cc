
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <x86intrin.h>

#include <atomic>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

#include "../include/atomic_wrapper.hh"
#include "../include/cache_line_size.hh"
#include "../include/cpu.hh"
#include "../include/debug.hh"
#include "../include/random.hh"
#include "../include/tsc.hh"
#include "../include/util.hh"

#define GiB 1073741824
#define MiB 1048576
#define KiB 1024

using std::cout;
using std::endl;

const uint64_t WORK_MEM_PER_THREAD = 256UL << 20;  // 256 MBytes.
const size_t PAGE_SIZE = 4096;

extern bool isReady(const std::vector<char> &readys);

extern void waitForReady(const std::vector<char> &readys);

extern void sleepMs(size_t ms);

class AlignedMemory {
  void *mem_;
  size_t size_;

public:
  AlignedMemory(size_t alignedSize, size_t allocateSize) {
    size_ = allocateSize;
    if (::posix_memalign(&mem_, alignedSize, allocateSize) != 0) {
      throw std::runtime_error("posix_memalign failed.");
    }
  }

  ~AlignedMemory() noexcept { ::free(mem_); }

  char *data() { return (char *) mem_; }

  const char *data() const { return (const char *) mem_; }

  size_t size() const { return size_; }
};

void fillArray(void *data, size_t size) {
  assert(size % CACHE_LINE_SIZE == 0);
  char *p = (char *) data;
  char *goal = p + size;
  alignas(CACHE_LINE_SIZE) const uint64_t buf[8] = {1, 0, 0, 0, 0, 0, 0, 0};
  while (p < goal) {
    memcpy(p, &buf[0], sizeof(buf));
    p += sizeof(buf);
  }
}

void writeFromCacheline(void *dst, const void *src, size_t size) {
  char *p = (char *) dst;
  while (size > CACHE_LINE_SIZE) {
    ::memcpy(p, src, CACHE_LINE_SIZE);
    p += CACHE_LINE_SIZE;
    size -= CACHE_LINE_SIZE;
  }
  ::memcpy(p, src, size);
}

struct Config {
  size_t nr_threads;
  size_t run_sec;
  const char *workload;
  size_t bulk_size;
};

enum class WorkloadType {
  Unknown,
  SeqRead,
  SeqWrite,
  RndRead,
  RndWrite,
};

WorkloadType getWorkloadType(const char *name) {
  struct {
    WorkloadType type;
    const char *name;
  } tbl[] = {
          {WorkloadType::Unknown,  "unknown"},
          {WorkloadType::SeqRead,  "seq_read"},
          {WorkloadType::SeqWrite, "seq_write"},
          {WorkloadType::RndRead,  "rnd_read"},
          {WorkloadType::RndWrite, "rnd_write"},
  };

  for (size_t i = 0; i < sizeof(tbl) / sizeof(tbl[0]); ++i) {
    if (::strncmp(tbl[i].name, name, 20) == 0) {
      return tbl[i].type;
    }
  }
  return WorkloadType::Unknown;
}

void flush_cachelines(void *data, size_t size) {
  // Assume cache line size is 64 bytes.
  uintptr_t addr = (uintptr_t) data;
  char *goal = (char *) (addr + size);
  addr &= ~(CACHE_LINE_SIZE - 1);  // aligned pointer.
  char *p = (char *) addr;
  while (p < goal) {
#if 1
    _mm_clwb(p);
#else
    _mm_clflushopt(p);
#endif
    p += CACHE_LINE_SIZE;
  }
  _mm_sfence();
}

void seqReadWorker(size_t idx, size_t bulk_size, char &ready, const bool &start,
                   const bool &quit, uint64_t &count) try {
  setThreadAffinity(idx);

  const uint64_t size = WORK_MEM_PER_THREAD;
  assert(size % bulk_size == 0);

  AlignedMemory mem(PAGE_SIZE, size);
  fillArray(mem.data(), mem.size());

  uint64_t transferred = 0;
  AlignedMemory buf(PAGE_SIZE, bulk_size);
  const char *p = (const char *) mem.data();
  const char *goal = p + size;

  storeRelease(ready, 1);
  while (!loadAcquire(start)) _mm_pause();

  while (!loadAcquire(quit)) {
    ::memcpy(buf.data(), p, bulk_size);
    transferred += bulk_size;

    p += bulk_size;
    if (p >= goal) p = (const char *) mem.data();
  }

  storeRelease(count, transferred);
} catch (std::exception &e) {
  ::fprintf(::stderr, "seqReadWorker error: %s\n", e.what());
}

void seqWriteWorker(size_t idx, size_t bulk_size, char &ready,
                    const bool &start, const bool &quit, uint64_t &count) try {
  setThreadAffinity(idx);

  const uint64_t size = WORK_MEM_PER_THREAD;
  assert(size % bulk_size == 0);

  AlignedMemory mem(PAGE_SIZE, size);

  fillArray(mem.data(), mem.size());

  AlignedMemory buf(PAGE_SIZE, bulk_size);
  uint64_t transferred = 0;
  char *p = (char *) mem.data();
  char *goal = p + size;

  storeRelease(ready, 1);
  while (!loadAcquire(start)) _mm_pause();
  while (!loadAcquire(quit)) {
    ::memcpy(p, buf.data(), bulk_size);
    flush_cachelines(p, bulk_size);

    p += bulk_size;
    transferred += bulk_size;
    if (p >= goal) p = (char *) mem.data();
  }

  storeRelease(count, transferred);
} catch (std::exception &e) {
  ::fprintf(::stderr, "seqWriteWorker error: %s\n", e.what());
}

bool is_power_of_2(uint64_t value) { return _popcnt64(value) == 1; }

void verify_power_of_2(uint64_t value) {
  if (!is_power_of_2(value)) {
    throw std::runtime_error("value is not power of 2");
  }
}

/**
 * random read worker
 */
void rndReadWorker(size_t idx, size_t bulk_size, char &ready, const bool &start,
                   const bool &quit, uint64_t &count) try {
  setThreadAffinity(idx);
  Xoroshiro128Plus rand;
  rand.init();

  const uint64_t size = WORK_MEM_PER_THREAD;
  const size_t max_pos = WORK_MEM_PER_THREAD / bulk_size;
  verify_power_of_2(max_pos);
  const size_t pos_mask =
          max_pos - 1;  // ex. max_pos: 00001000 --> pos_mask: 00000111

  AlignedMemory mem(PAGE_SIZE, size);
  fillArray(mem.data(), mem.size());
  uint64_t transferred = 0;
  AlignedMemory buf(PAGE_SIZE, bulk_size);
  const char *base = (const char *) mem.data();

  storeRelease(ready, 1);
  while (!loadAcquire(start)) _mm_pause();

  while (!loadAcquire(quit)) {
    const size_t pos = rand() & pos_mask;
    const char *p = base + (pos * bulk_size);

    ::memcpy(buf.data(), p, bulk_size);

    p += bulk_size;
    transferred += bulk_size;
  }

  storeRelease(count, transferred);
} catch (std::exception &e) {
  ::fprintf(::stderr, "seqReadWorker error: %s\n", e.what());
}

void rndWriteWorker(size_t idx, size_t bulk_size, char &ready,
                    const bool &start, const bool &quit, uint64_t &count) try {
  setThreadAffinity(idx);
  Xoroshiro128Plus rand;
  rand.init();

  const uint64_t size = WORK_MEM_PER_THREAD;
  const size_t max_pos = WORK_MEM_PER_THREAD / bulk_size;
  verify_power_of_2(max_pos);
  const size_t pos_mask =
          max_pos - 1;  // ex. max_pos: 00001000 00> pos_mask: 00000111

  AlignedMemory mem(PAGE_SIZE, size);
  fillArray(mem.data(), mem.size());
  uint64_t transferred = 0;
  AlignedMemory buf(PAGE_SIZE, bulk_size);
  char *base = (char *) mem.data();

  storeRelease(ready, 1);
  while (!loadAcquire(start)) _mm_pause();
  while (!loadAcquire(quit)) {
    const size_t pos = rand() & pos_mask;
    char *p = base + (pos * bulk_size);

    ::memcpy(p, buf.data(), bulk_size);
    flush_cachelines(p, bulk_size);

    p += bulk_size;
    transferred += bulk_size;
  }
  storeRelease(count, transferred);
} catch (std::exception &e) {
  ::fprintf(::stderr, "seqWriteWorker error: %s\n", e.what());
}

void runExpr(const Config &cfg) {
  bool start = false;
  bool quit = false;
  std::vector<char> readys(cfg.nr_threads);
  std::vector <uint64_t> counts(cfg.nr_threads);
  std::vector <std::thread> thv;
  for (size_t i = 0; i < cfg.nr_threads; ++i) {
    switch (getWorkloadType(cfg.workload)) {
      case WorkloadType::SeqRead:
        thv.emplace_back(seqReadWorker, i, cfg.bulk_size, std::ref(readys[i]),
                         std::ref(start), std::ref(quit), std::ref(counts[i]));
        break;
      case WorkloadType::SeqWrite:
        thv.emplace_back(seqWriteWorker, i, cfg.bulk_size, std::ref(readys[i]),
                         std::ref(start), std::ref(quit), std::ref(counts[i]));
        break;
      case WorkloadType::RndRead:
        thv.emplace_back(rndReadWorker, i, cfg.bulk_size, std::ref(readys[i]),
                         std::ref(start), std::ref(quit), std::ref(counts[i]));
        break;
      case WorkloadType::RndWrite:
        thv.emplace_back(rndWriteWorker, i, cfg.bulk_size, std::ref(readys[i]),
                         std::ref(start), std::ref(quit), std::ref(counts[i]));
        break;
      default:
        throw std::runtime_error("unknown worklaod");
    }
  }
  waitForReady(readys);
  storeRelease(start, true);
  for (size_t i = 0; i < cfg.run_sec; ++i) {
    sleepMs(1000);
  }
  storeRelease(quit, true);
  for (auto &th : thv) th.join();

  uint64_t total = 0;
  for (uint64_t c : counts) {
    total += c;
  }

  const double latency_ns =
          (1000000000.0 * cfg.nr_threads * cfg.run_sec * cfg.bulk_size) /
          (double) (total);
  ::printf(
          "nr_threads %zu run_sec %zu workload %-10s bulk_size %4zu Bps %15"
  PRIu64
  " ops %15"
  PRIu64
  " latency_ns %5.3f\n",
          cfg.nr_threads, cfg.run_sec, cfg.workload, cfg.bulk_size,
          total / cfg.run_sec, total / cfg.bulk_size / cfg.run_sec, latency_ns);
  ::fflush(::stdout);
}

void put8(const uint64_t *p) {
  for (size_t i = 0; i < 24; ++i) {
    ::printf("%"
    PRIu64
    "", p[i]);
  }
  ::printf("\n");
}

int main(int argc, char *argv[]) try {
  if (argc != 2) ERR;
  size_t nr_threads = atoi(argv[1]);

  size_t run_sec = 3;
  size_t nr_loop = 1;

  for (const char *workload :
          {"seq_read", "seq_write", "rnd_read", "rnd_write"}) {
    // for (const char *workload : {"seq_write"}) {
    // for (size_t bulk_size : {8, 64, 1024, 4096}) {
    for (size_t bulk_size : {64}) {
      assert(bulk_size % sizeof(uint64_t) == 0);
      for (size_t i = 0; i < nr_loop; ++i) {
        runExpr({nr_threads, run_sec, workload, bulk_size});
      }
    }
  }
} catch (std::exception &e) {
  ::fprintf(::stderr, "main error: %s\n", e.what());
  ::exit(1);
}
