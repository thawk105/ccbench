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
#include "include/result.hh"
#include "include/transaction.hh"
#include "include/util.hh"

using namespace std;

void worker(size_t thid, char &ready, const bool &start, const bool &quit) {
  Xoroshiro128Plus rnd;
  rnd.init();
  TxExecutor trans(thid, &rnd, (Result *) &MoccResult[thid]);
  FastZipf zipf(&rnd, FLAGS_zipf_skew, FLAGS_tuple_num);
  uint64_t epoch_timer_start, epoch_timer_stop;
  Backoff backoff(FLAGS_clocks_per_us);
  Result &myres = std::ref(MoccResult[thid]);

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
    makeProcedure(trans.pro_set_, rnd, zipf, FLAGS_tuple_num, FLAGS_max_ope, FLAGS_thread_num,
                  FLAGS_rratio, FLAGS_rmw, FLAGS_ycsb, false, thid, myres);
RETRY:
    if (thid == 0) {
      leaderWork(epoch_timer_start, epoch_timer_stop, myres);
      leaderBackoffWork(backoff, MoccResult);
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
    /**
     * local_commit_counts is used at ../include/backoff.hh to calcurate about
     * backoff.
     */
    storeRelease(myres.local_commit_counts_,
                 loadAcquire(myres.local_commit_counts_) + 1);
  }

  return;
}

int main(int argc, char *argv[]) try {
  gflags::SetUsageMessage("MOCC benchmark.");
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  chkArg();
  makeDB();

  alignas(CACHE_LINE_SIZE) bool start = false;
  alignas(CACHE_LINE_SIZE) bool quit = false;
  initResult();
  std::vector<char> readys(FLAGS_thread_num);
  std::vector<std::thread> thv;
  for (size_t i = 0; i < FLAGS_thread_num; ++i)
    thv.emplace_back(worker, i, std::ref(readys[i]), std::ref(start),
                     std::ref(quit));
  waitForReady(readys);
  storeRelease(start, true);
  for (size_t i = 0; i < FLAGS_extime; ++i) {
    sleepMs(1000);
  }
  storeRelease(quit, true);
  for (auto &th : thv) th.join();

  for (unsigned int i = 0; i < FLAGS_thread_num; ++i) {
    MoccResult[0].addLocalAllResult(MoccResult[i]);
  }
  MoccResult[0].displayAllResult(FLAGS_clocks_per_us, FLAGS_extime, FLAGS_thread_num);

  return 0;
} catch (bad_alloc) {
  ERR;
}
