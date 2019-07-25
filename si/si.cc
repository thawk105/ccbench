
#include <ctype.h>  //isdigit, 
#include <pthread.h>
#include <string.h> //strlen,
#include <sys/syscall.h>  //syscall(SYS_gettid),  
#include <sys/types.h>  //syscall(SYS_gettid),
#include <time.h>
#include <unistd.h> //syscall(SYS_gettid), 
#include <iostream>
#include <string> //string

#define GLOBAL_VALUE_DEFINE

#include "../include/cpu.hh"
#include "../include/debug.hh"
#include "../include/int64byte.hh"
#include "../include/procedure.hh"
#include "../include/random.hh"
#include "../include/tsc.hh"
#include "../include/zipf.hh"
#include "include/common.hh"
#include "include/garbage_collection.hh"
#include "include/result.hh"
#include "include/transaction.hh"

using namespace std;

extern void chkArg(const int argc, const char *argv[]);
extern bool chkClkSpan(const uint64_t start, const uint64_t stop, const uint64_t threshold);
extern void makeDB();
extern void makeProcedure(std::vector<Procedure>& pro, Xoroshiro128Plus &rnd, FastZipf &zipf, size_t tuple_num, size_t max_ope, size_t rratio, bool rmw, bool ycsb);
extern void naiveGarbageCollection();
extern void ReadyAndWaitForReadyOfAllThread(std::atomic<size_t> &running, size_t thnm);
extern void waitForReadyOfAllThread(std::atomic<size_t> &running, size_t thnm);
extern void sleepMs(size_t ms);

static void *
manager_worker(void *arg)
{
  SIResult &res = *(SIResult *)(arg);
  GarbageCollection gcobject_;
 
#ifdef Linux 
  setThreadAffinity(res.thid_);
#endif // Linux

  gcobject_.decideFirstRange();
  ReadyAndWaitForReadyOfAllThread(Running, THREAD_NUM);
  
  for (;;) {
    usleep(1);

    if (gcobject_.chkSecondRange()) {
      gcobject_.decideGcThreshold();
      gcobject_.mvSecondRangeToFirstRange();
    }
    if (Result::Finish_.load(std::memory_order_acquire)) return nullptr;
  }

  return nullptr;
}


static void *
worker(void *arg)
{
  SIResult &res = *(SIResult *)(arg);
  TxExecutor trans(res.thid_, MAX_OPE, (SIResult*)arg);
  ReadyAndWaitForReadyOfAllThread(Running, THREAD_NUM);
  Xoroshiro128Plus rnd;
  rnd.init();
  FastZipf zipf(&rnd, ZIPF_SKEW, TUPLE_NUM);

#if MASSTREE_USE
 MasstreeWrapper<Tuple>::thread_init(int(res.thid_));
#endif

#ifdef Linux
  setThreadAffinity(res.thid_);
  //printf("Thread #%d: on CPU %d\n", *myid, sched_getcpu());
  //printf("sysconf(_SC_NPROCESSORS_CONF) %ld\n", sysconf(_SC_NPROCESSORS_CONF));
#endif // Linux

  //start work (transaction)
  //printf("Thread #%d: on CPU %d\n", *myid, sched_getcpu());
  for(;;) {
    makeProcedure(trans.proSet_, rnd, zipf, TUPLE_NUM, MAX_OPE, RRATIO, RMW, YCSB);
RETRY:

    if (Result::Finish_.load(std::memory_order_acquire)) {
      return nullptr;
    }

    trans.tbegin();
    for (auto itr = trans.proSet_.begin(); itr != trans.proSet_.end(); ++itr) {
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

#if 1
    // maintenance phase
    // garbage collection
    uint32_t loadThreshold = trans.gcobject_.getGcThreshold();
    if (trans.pre_gc_threshold_ != loadThreshold) {
      trans.gcobject_.gcVersion(trans.sres_);
      trans.pre_gc_threshold_ = loadThreshold;
#ifdef CCTR_ON
      trans.gcobject_.gcTMTElements(trans.sres_);
#endif // CCTR_ON
      ++trans.sres_->local_gc_counts_;
    }
#endif
    }

  return nullptr;
}

int
main(const int argc, const char *argv[]) try
{
  chkArg(argc, argv);
  makeDB();

  SIResult rsob[THREAD_NUM];
  SIResult &rsroot = rsob[0];
  pthread_t thread[THREAD_NUM];
  for (unsigned int i = 0; i < THREAD_NUM; ++i) {
    int ret;
    rsob[i].thid_ = i;
    if (i == 0)
      ret = pthread_create(&thread[i], NULL, manager_worker, (void *)(&rsob[i]));
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
    rsroot.addLocalAllSIResult(rsob[i]);
  }

  rsroot.extime_ = EXTIME;
  rsroot.displayAllSIResult();

  return 0;
} catch (bad_alloc) {
  ERR;
}
