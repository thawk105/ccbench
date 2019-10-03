
#include <ctype.h>
#include <pthread.h>
#include <sched.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <x86intrin.h>

#include <algorithm>
#include <cctype>

#define GLOBAL_VALUE_DEFINE

#include "../include/atomic_wrapper.hh"
#include "../include/backoff.hh"
#include "../include/cpu.hh"
#include "../include/debug.hh"
#include "../include/masstree_wrapper.hh"
#include "../include/random.hh"
#include "../include/result.hh"
#include "../include/tsc.hh"
#include "../include/util.hh"
#include "../include/zipf.hh"
#include "include/common.hh"
#include "include/transaction.hh"

extern void chkArg(const int argc, char* argv[]);
extern bool chkClkSpan(const uint64_t start, const uint64_t stop,
                       const uint64_t threshold);
extern void displayDB();
extern void displayPRO();
extern void isReady(const std::vector<char>& readys);
extern void makeDB();
extern void sleepMs(size_t ms);
extern void waitForReady(const std::vector<char>& readys);
void worker(size_t thid, char& ready, const bool& start, const bool& quit,
            std::vector<Result>& res) {
  Xoroshiro128Plus rnd;
  rnd.init();
  TxExecutor trans(thid, (Result*)&res[thid]);
  FastZipf zipf(&rnd, ZIPF_SKEW, TUPLE_NUM);

#if BACK_OFF
  Backoff backoff(CLOCKS_PER_US);
#endif

#if MASSTREE_USE
  MasstreeWrapper<Tuple>::thread_init(int(thid));
#endif

#ifdef Linux
  setThreadAffinity(thid);
  //size_t cpu_id = thid / 4 + thid % 4 * 28;
  //setThreadAffinity(cpu_id);
  // printf("Thread %zu, affi %zu\n", thid, cpu_id);
  // printf("Thread #%d: on CPU %d\n", *myid, sched_getcpu());
  // printf("sysconf(_SC_NPROCESSORS_CONF) %d\n",
  // sysconf(_SC_NPROCESSORS_CONF));
#endif

  storeRelease(ready, 1);
  while (!loadAcquire(start)) _mm_pause();
  while (!loadAcquire(quit)) {
    makeProcedure(trans.pro_set_, rnd, zipf, TUPLE_NUM, MAX_OPE, THREAD_NUM,
                  RRATIO, RMW, YCSB, false, thid, std::ref(res[thid]));
  RETRY:
#if BACK_OFF
    if (thid == 0) leaderBackoffWork(std::ref(backoff), std::ref(res));
#endif
    if (loadAcquire(quit)) break;

    trans.begin();
    for (auto itr = trans.pro_set_.begin(); itr != trans.pro_set_.end();
         ++itr) {
      if ((*itr).ope_ == Ope::READ) {
        trans.read((*itr).key_);
      } else if ((*itr).ope_ == Ope::WRITE) {
        trans.write((*itr).key_);
      } else if ((*itr).ope_ == Ope::READ_MODIFY_WRITE) {
        trans.read((*itr).key_);
        trans.write((*itr).key_);
      } else {
        ERR;
      }

      if (trans.status_ == TransactionStatus::aborted) {
        trans.abort();
        goto RETRY;
      }
    }

    if (trans.validationPhase()) {
      trans.writePhase();
    } else {
      trans.abort();
      goto RETRY;
    }
  }

  return;
}

int main(int argc, char* argv[]) try {
  chkArg(argc, argv);
  makeDB();

  alignas(CACHE_LINE_SIZE) bool start = false;
  alignas(CACHE_LINE_SIZE) bool quit = false;
  alignas(CACHE_LINE_SIZE) std::vector<Result> res(THREAD_NUM);
  std::vector<char> readys(THREAD_NUM);
  std::vector<std::thread> thv;
  for (size_t i = 0; i < THREAD_NUM; ++i)
    thv.emplace_back(worker, i, std::ref(readys[i]), std::ref(start),
                     std::ref(quit), std::ref(res));
  waitForReady(readys);
  storeRelease(start, true);
  for (size_t i = 0; i < EXTIME; ++i) {
    sleepMs(1000);
  }
  storeRelease(quit, true);
  for (auto& th : thv) th.join();

  for (unsigned int i = 0; i < THREAD_NUM; ++i) {
    res[0].addLocalAllResult(res[i]);
  }
  res[0].displayAllResult(CLOCKS_PER_US, EXTIME, THREAD_NUM);

  return 0;
} catch (bad_alloc) {
  ERR;
}
