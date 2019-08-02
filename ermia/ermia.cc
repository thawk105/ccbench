
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

#include "../include/cpu.hh"
#include "../include/debug.hh"
#include "../include/int64byte.hh"
#include "../include/masstree_wrapper.hh"
#include "../include/procedure.hh"
#include "../include/random.hh"
#include "../include/result.hh"
#include "../include/tsc.hh"
#include "../include/zipf.hh"
#include "include/common.hh"
#include "include/garbage_collection.hh"
#include "include/transaction.hh"

using namespace std;

extern void chkArg(const int argc, const char *argv[]);
extern bool chkClkSpan(const uint64_t start, const uint64_t stop,
                       const uint64_t threshold);
extern void makeDB();
extern void makeProcedure(std::vector<Procedure> &pro, Xoroshiro128Plus &rnd,
                          FastZipf &zipf, size_t tuple_num, size_t max_ope,
                          size_t thread_num, size_t rratio, bool rmw, bool ycsb,
                          bool partition, size_t thread_id);
extern void naiveGarbageCollection();
extern void ReadyAndWaitForReadyOfAllThread(std::atomic<size_t> &running,
                                            size_t thnm);
extern void waitForReadyOfAllThread(std::atomic<size_t> &running, size_t thnm);
extern void sleepMs(size_t);

static void *manager_worker(void *arg) {
  Result &res = *(Result *)(arg);
  GarbageCollection gcobject;

#ifdef Linux
  setThreadAffinity(res.thid_);
#endif  // Linux

  gcobject.decideFirstRange();
  ReadyAndWaitForReadyOfAllThread(Running, THREAD_NUM);
  // end, initial work

  for (;;) {
    if (Result::Finish_.load(std::memory_order_acquire)) return nullptr;

    usleep(1);
    if (gcobject.chkSecondRange()) {
      gcobject.decideGcThreshold();
      gcobject.mvSecondRangeToFirstRange();
    }
  }

  return nullptr;
}

static void *worker(void *arg) {
  Result &res = *(Result *)(arg);
  TxExecutor trans(res.thid_, MAX_OPE, (Result *)arg);
  Xoroshiro128Plus rnd;
  rnd.init();
  FastZipf zipf(&rnd, ZIPF_SKEW, TUPLE_NUM);

#if MASSTREE_USE
  MasstreeWrapper<Tuple>::thread_init(int(res.thid_));
#endif

#ifdef Linux
  setThreadAffinity(res.thid_);
  // printf("Thread #%zu: on CPU %d\n", res.thid_, sched_getcpu());
  // printf("sysconf(_SC_NPROCESSORS_CONF) %ld\n",
  // sysconf(_SC_NPROCESSORS_CONF));
#endif  // Linux
  // printf("Thread #%d: on CPU %d\n", *myid, sched_getcpu());

  ReadyAndWaitForReadyOfAllThread(Running, THREAD_NUM);
  trans.gcstart_ = rdtscp();
  for (;;) {
    makeProcedure(trans.pro_set_, rnd, zipf, TUPLE_NUM, MAX_OPE, THREAD_NUM,
                  RRATIO, RMW, YCSB, false, res.thid_);
  RETRY:
    if (Result::Finish_.load(std::memory_order_acquire)) return nullptr;

    //-----
    // transaction begin
    trans.tbegin();
    for (auto itr = trans.pro_set_.begin(); itr != trans.pro_set_.end();
         ++itr) {
      if ((*itr).ope_ == Ope::READ) {
        trans.ssn_tread((*itr).key_);
      } else if ((*itr).ope_ == Ope::WRITE) {
        trans.ssn_twrite((*itr).key_);
      } else if ((*itr).ope_ == Ope::READ_MODIFY_WRITE) {
        trans.ssn_tread((*itr).key_);
        trans.ssn_twrite((*itr).key_);
      } else {
        ERR;
      }

      if (trans.status_ == TransactionStatus::aborted) {
        trans.abort();
        goto RETRY;
      }
    }

    trans.ssn_parallel_commit();

    if (trans.status_ == TransactionStatus::aborted) {
      trans.abort();
      goto RETRY;
    }

    // maintenance phase
    // garbage collection
    trans.mainte();
  }

  return nullptr;
}

int main(const int argc, const char *argv[]) try {
  chkArg(argc, argv);
  makeDB();

  Result rsob[THREAD_NUM];
  Result &rsroot = rsob[0];
  pthread_t thread[THREAD_NUM];

  for (unsigned int i = 0; i < THREAD_NUM; ++i) {
    int ret;
    rsob[i] = Result(CLOCKS_PER_US, EXTIME, i, THREAD_NUM);
    if (i == 0)
      ret =
          pthread_create(&thread[i], NULL, manager_worker, (void *)(&rsob[i]));
    else
      ret = pthread_create(&thread[i], NULL, worker, (void *)(&rsob[i]));
    if (ret) ERR;
  }

  waitForReadyOfAllThread(Running, THREAD_NUM);
  for (size_t i = 0; i < EXTIME; ++i) {
    sleepMs(1000);
  }
  Result::Finish_.store(true, std::memory_order_release);

  for (unsigned int i = 0; i < THREAD_NUM; ++i) {
    pthread_join(thread[i], nullptr);
    rsroot.addLocalAllResult(rsob[i]);
  }
  rsroot.displayAllResult();

  return 0;
} catch (bad_alloc) {
  ERR;
}
