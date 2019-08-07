
#include <stdio.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <xmmintrin.h>

#include <atomic>
#include <thread>
#include <vector>

#include "../include/atomic_wrapper.hh"
#include "../include/debug.hh"
#include "../include/procedure.hh"
#include "../include/random.hh"
#include "../include/result.hh"
#include "../include/zipf.hh"

bool chkSpan(struct timeval &start, struct timeval &stop, long threshold) {
  long diff = 0;
  diff += (stop.tv_sec - start.tv_sec) * 1000 * 1000 +
          (stop.tv_usec - start.tv_usec);
  if (diff > threshold)
    return true;
  else
    return false;
}

bool chkClkSpan(const uint64_t start, const uint64_t stop,
                const uint64_t threshold) {
  uint64_t diff = 0;
  diff = stop - start;
  if (diff > threshold)
    return true;
  else
    return false;
}

size_t decideParallelBuildNumber(size_t tuple_num) {
  // if table size is very small, it builds by single thread.
  if (tuple_num < 1000) return 1;

  // else
  for (size_t i = std::thread::hardware_concurrency(); i > 0; --i) {
    if (tuple_num % i == 0) {
      return i;
    }
    if (i == 1) ERR;
  }

  return 0;
}

void displayProcedureVector(std::vector<Procedure> &pro) {
  printf("--------------------\n");
  size_t index = 0;
  for (auto itr = pro.begin(); itr != pro.end(); ++itr) {
    printf(
        "-----\n"
        "op_num\t: %zu\n"
        "key\t: %zu\n"
        "r/w\t: %d\n",
        index, (*itr).key_, (int)((*itr).ope_));
    ++index;
  }
  printf("--------------------\n");
}

void displayRusageRUMaxrss() {
  struct rusage r;
  if (getrusage(RUSAGE_SELF, &r) != 0) ERR;
  printf("maxrss:\t%ld kB\n", r.ru_maxrss);
}

void ReadyAndWaitForReadyOfAllThread(std::atomic<size_t> &running,
                                     const size_t thnm) {
  running++;
  while (running.load(std::memory_order_acquire) != thnm) _mm_pause();

  return;
}

void waitForReadyOfAllThread(std::atomic<size_t> &running, const size_t thnm) {
  while (running.load(std::memory_order_acquire) != thnm) _mm_pause();

  return;
}

bool isReady(const std::vector<char> &readys) {
  for (const char &b : readys) {
    if (!loadAcquire(b)) return false;
  }
  return true;
}

void waitForReady(const std::vector<char> &readys) {
  while (!isReady(readys)) {
    _mm_pause();
  }
}

void sleepMs(size_t ms) {
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}
