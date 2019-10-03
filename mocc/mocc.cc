#include <ctype.h>  // isdigit,
#include <pthread.h>
#include <string.h>       // strlen,
#include <sys/syscall.h>  // syscall(SYS_gettid),
#include <sys/types.h>    // syscall(SYS_gettid),
#include <time.h>
#include <unistd.h>  // syscall(SYS_gettid),
#include <iostream>
#include <string>  // string

#define GLOBAL_VALUE_DEFINE

#include "../include/atomic_wrapper.hh"
#include "../include/backoff.hh"
#include "../include/cpu.hh"
#include "../include/debug.hh"
#include "../include/int64byte.hh"
#include "../include/masstree_wrapper.hh"
#include "../include/result.hh"
#include "../include/tsc.hh"
#include "../include/util.hh"
#include "../include/zipf.hh"
#include "include/atomic_tool.hh"
#include "include/common.hh"
#include "include/transaction.hh"

using namespace std;

extern void chkArg(const int argc, char* argv[]);
extern bool chkClkSpan(const uint64_t start, const uint64_t stop,
                       const uint64_t threshold);
extern void isReady(const std::vector<char>& readys);
extern void leaderWork(uint64_t& epoch_timer_start, uint64_t& epoch_timer_stop,
                       Result& res);
extern void displayDB();
extern void displayLockedTuple();
extern void displayPRO();
extern void makeDB();
extern void waitForReady(const std::vector<char>& readys);
extern void sleepMs(size_t ms);

void worker(size_t thid, char& ready, const bool& start, const bool& quit,
            std::vector<Result>& res) {
  Xoroshiro128Plus rnd;
  rnd.init();
  TxExecutor trans(thid, &rnd, (Result*)&res[thid]);
  FastZipf zipf(&rnd, ZIPF_SKEW, TUPLE_NUM);
  uint64_t epoch_timer_start, epoch_timer_stop;
  Backoff backoff(CLOCKS_PER_US);
  Result& myres = std::ref(res[thid]);

#if MASSTREE_USE
  MasstreeWrapper<Tuple>::thread_init(int(thid));
#endif

#ifdef Linux
  setThreadAffinity(thid);
#endif  // Linux

  storeRelease(ready, 1);
  while (!loadAcquire(start)) _mm_pause();
  if (thid == 0) epoch_timer_start = rdtscp();
  while (!loadAcquire(quit)) {
    makeProcedure(trans.pro_set_, rnd, zipf, TUPLE_NUM, MAX_OPE, THREAD_NUM,
                  RRATIO, RMW, YCSB, false, thid, myres);
  RETRY:
    if (thid == 0) {
      leaderWork(epoch_timer_start, epoch_timer_stop, myres);
      leaderBackoffWork(backoff, res);
    }
    if (loadAcquire(quit)) break;

    trans.begin();
    for (auto itr = trans.pro_set_.begin(); itr != trans.pro_set_.end();
         ++itr) {
      if ((*itr).ope_ == Ope::READ) {
        trans.read((*itr).key_);
      } else if ((*itr).ope_ == Ope::WRITE) {
        trans.write((*itr).key_);
      } else if ((*itr).ope_ == Ope::READ_MODIFY_WRITE) {
        // trans.read((*itr).key_);
        // trans.write((*itr).key_);
        trans.read_write((*itr).key_);
      } else {
        ERR;
      }

      if (trans.status_ == TransactionStatus::aborted) {
        trans.abort();
#if ADD_ANALYSIS
        ++myres.local_abort_by_operation_;
#endif
        goto RETRY;
      }
    }

    if (!(trans.commit())) {
      trans.abort();
#if ADD_ANALYSIS
      ++myres.local_abort_by_validation_;
#endif
      goto RETRY;
    }

    trans.writePhase();
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
