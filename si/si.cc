
#include <ctype.h>  //isdigit,
#include <pthread.h>
#include <string.h>       //strlen,
#include <sys/syscall.h>  //syscall(SYS_gettid),
#include <sys/types.h>    //syscall(SYS_gettid),
#include <time.h>
#include <unistd.h>  //syscall(SYS_gettid),
#include <iostream>
#include <string>  //string

#define GLOBAL_VALUE_DEFINE

#include "../include/backoff.hh"
#include "../include/cpu.hh"
#include "../include/debug.hh"
#include "../include/int64byte.hh"
#include "../include/procedure.hh"
#include "../include/random.hh"
#include "../include/result.hh"
#include "../include/tsc.hh"
#include "../include/util.hh"
#include "../include/zipf.hh"
#include "include/common.hh"
#include "include/garbage_collection.hh"
#include "include/result.hh"
#include "include/transaction.hh"
#include "include/util.hh"

using namespace std;

void worker(size_t thid, char& ready, const bool& start, const bool& quit) {
  Result& myres = std::ref(SIResult[thid]);
  TxExecutor trans(thid, MAX_OPE, (Result*)&myres);
  Xoroshiro128Plus rnd;
  rnd.init();
  FastZipf zipf(&rnd, ZIPF_SKEW, TUPLE_NUM);
  GarbageCollection gcob;
  Backoff backoff(CLOCKS_PER_US);

#if MASSTREE_USE
  MasstreeWrapper<Tuple>::thread_init(int(thid));
#endif

#ifdef Linux
  setThreadAffinity(thid);
  // printf("Thread #%d: on CPU %d\n", *myid, sched_getcpu());
  // printf("sysconf(_SC_NPROCESSORS_CONF) %ld\n",
  // sysconf(_SC_NPROCESSORS_CONF));
#endif  // Linux

  if (thid == 0) gcob.decideFirstRange();
  storeRelease(ready, 1);
  while (!loadAcquire(start)) _mm_pause();
  trans.gcstart_ = rdtscp();
  while (!loadAcquire(quit)) {
    makeProcedure(trans.pro_set_, rnd, zipf, TUPLE_NUM, MAX_OPE, THREAD_NUM,
                  RRATIO, RMW, YCSB, false, thid, myres);
  RETRY:
    if (thid == 0) {
      leaderWork(std::ref(gcob));
      leaderBackoffWork(backoff, SIResult);
    }
    if (loadAcquire(quit)) break;

    trans.tbegin();
    for (auto itr = trans.pro_set_.begin(); itr != trans.pro_set_.end();
         ++itr) {
      if ((*itr).ope_ == Ope::READ) {
        trans.tread((*itr).key_);
      } else if ((*itr).ope_ == Ope::WRITE) {
        trans.twrite((*itr).key_);
      } else if ((*itr).ope_ == Ope::READ_MODIFY_WRITE) {
        trans.tread((*itr).key_);
        trans.twrite((*itr).key_);
      } else {
        ERR;
      }

      if (trans.status_ == TransactionStatus::aborted) {
        trans.abort();
        goto RETRY;
      }
    }

    trans.commit();
    /**
     * local_commit_counts is used at ../include/backoff.hh to calcurate about
     * backoff.
     */
    storeRelease(myres.local_commit_counts_,
                 loadAcquire(myres.local_commit_counts_) + 1);

    // maintenance phase
    // garbage collection
    trans.mainte();
  }

  return;
}

int main(const int argc, const char* argv[]) try {
  chkArg(argc, argv);
  makeDB();

  alignas(CACHE_LINE_SIZE) bool start = false;
  alignas(CACHE_LINE_SIZE) bool quit = false;
  initResult();
  std::vector<char> readys(THREAD_NUM);
  std::vector<std::thread> thv;
  for (size_t i = 0; i < THREAD_NUM; ++i)
    thv.emplace_back(worker, i, std::ref(readys[i]), std::ref(start),
                     std::ref(quit));
  waitForReady(readys);
  storeRelease(start, true);
  for (size_t i = 0; i < EXTIME; ++i) {
    sleepMs(1000);
  }
  storeRelease(quit, true);
  for (auto& th : thv) th.join();

  for (unsigned int i = 0; i < THREAD_NUM; ++i) {
    SIResult[0].addLocalAllResult(SIResult[i]);
  }
  SIResult[0].displayAllResult(CLOCKS_PER_US, EXTIME, THREAD_NUM);

  return 0;
} catch (bad_alloc) {
  ERR;
}
