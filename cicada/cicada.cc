#include <ctype.h>
#include <pthread.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sched.h>
#include <unistd.h>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <random>
#include <thread>

#define GLOBAL_VALUE_DEFINE
#include "../include/cpu.hh"
#include "../include/debug.hh"
#include "../include/int64byte.hh"
#include "../include/procedure.hh"
#include "../include/random.hh"
#include "../include/zipf.hh"
#include "include/common.hh"
#include "include/result.hh"
#include "include/transaction.hh"

using namespace std;

extern void chkArg(const int argc, char *argv[]);
extern bool chkClkSpan(const uint64_t start, const uint64_t stop, const uint64_t threshold);
extern bool chkSpan(struct timeval &start, struct timeval &stop, long threshold);
extern void makeDB(uint64_t *initial_wts);
extern void makeProcedure(std::vector<Procedure>& pro, Xoroshiro128Plus &rnd, FastZipf &zipf, size_t tuple_num, size_t max_ope, size_t rratio, bool rmw, bool ycsb);
extern void ReadyAndWaitForReadyOfAllThread(std::atomic<size_t> &running, size_t thnm);
extern void waitForReadyOfAllThread(std::atomic<size_t> &running, size_t thnm);
extern void sleepMs(size_t ms);

static void *
manager_worker(void *arg)
{
  TimeStamp tmp;

  uint64_t initial_wts;
  makeDB(&initial_wts);
  MinWts.store(initial_wts + 2, memory_order_release);
  CicadaResult &res = *(CicadaResult *)(arg);

#ifdef Linux
  setThreadAffinity(res.thid_);
  //printf("Thread #%d: on CPU %d\n", *myid, sched_getcpu());
#endif // Linux

  ReadyAndWaitForReadyOfAllThread(Running, THREAD_NUM);
  while (FirstAllocateTimestamp.load(memory_order_acquire) != THREAD_NUM - 1) {}

  // leader work
  for(;;) {
    if (res.Finish_.load(std::memory_order_acquire))
      return nullptr;

    bool gc_update = true;
    for (unsigned int i = 1; i < THREAD_NUM; ++i) {
    //check all thread's flag raising
      if (__atomic_load_n(&(GCFlag[i].obj_), __ATOMIC_ACQUIRE) == 0) {
        usleep(1);
        gc_update = false;
        break;
      }
    }
    if (gc_update) {
      uint64_t minw = __atomic_load_n(&(ThreadWtsArray[1].obj_), __ATOMIC_ACQUIRE);
      uint64_t minr;
      if (GROUP_COMMIT == 0) {
        minr = __atomic_load_n(&(ThreadRtsArray[1].obj_), __ATOMIC_ACQUIRE);
      } else {
        minr = __atomic_load_n(&(ThreadRtsArrayForGroup[1].obj_), __ATOMIC_ACQUIRE);
      }

      for (unsigned int i = 1; i < THREAD_NUM; ++i) {
        uint64_t tmp = __atomic_load_n(&(ThreadWtsArray[i].obj_), __ATOMIC_ACQUIRE);
        if (minw > tmp) minw = tmp;
        if (GROUP_COMMIT == 0) {
          tmp = __atomic_load_n(&(ThreadRtsArray[i].obj_), __ATOMIC_ACQUIRE);
          if (minr > tmp) minr = tmp;
        } else {
          tmp = __atomic_load_n(&(ThreadRtsArrayForGroup[i].obj_), __ATOMIC_ACQUIRE);
          if (minr > tmp) minr = tmp;
        }
      }

      MinWts.store(minw, memory_order_release);
      MinRts.store(minr, memory_order_release);

      // downgrade gc flag
      for (unsigned int i = 1; i < THREAD_NUM; ++i) {
        __atomic_store_n(&(GCFlag[i].obj_), 0, __ATOMIC_RELEASE);
        __atomic_store_n(&(GCExecuteFlag[i].obj_), 1, __ATOMIC_RELEASE);
      }
    }
  }

  return nullptr;
}

static void *
worker(void *arg)
{
  CicadaResult &res = *(CicadaResult *)(arg);
  Xoroshiro128Plus rnd;
  rnd.init();
  TxExecutor trans(res.thid_, (CicadaResult*)arg);
  FastZipf zipf(&rnd, ZIPF_SKEW, TUPLE_NUM);

#ifdef Linux
  setThreadAffinity(res.thid_);
  //printf("Thread #%d: on CPU %d\n", *myid, sched_getcpu());
  //printf("sysconf(_SC_NPROCESSORS_CONF) %d\n", sysconf(_SC_NPROCESSORS_CONF));
#endif // Linux

#ifdef Darwin
  int nowcpu;
  GETCPU(nowcpu);
  //printf("Thread %d on CPU %d\n", *myid, nowcpu);
#endif // Darwin

  ReadyAndWaitForReadyOfAllThread(Running, THREAD_NUM);

  //printf("%s\n", trans.writeVal);
  //start work(transaction)
  for (;;) {
    makeProcedure(trans.proSet_, rnd, zipf, TUPLE_NUM, MAX_OPE, RRATIO, RMW, YCSB);

RETRY:
    if (res.Finish_.load(std::memory_order_acquire))
      return nullptr;

    trans.tbegin();
    //Read phase
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

      if (trans.status_ == TransactionStatus::abort) {
        trans.earlyAbort();
        goto RETRY;
      }
    }

    // read only tx doesn't collect read set and doesn't validate.
    // write phase execute logging and commit pending versions, but r-only tx can skip it.
    if ((*trans.proSet_.begin()).ronly_) {
      ++trans.continuing_commit_;
      ++res.local_commit_counts_;
    } else {
      //Validation phase
      if (!trans.validation()) {
        trans.abort();
        goto RETRY;
      }

      //Write phase
      trans.writePhase();

      //Maintenance
      //Schedule garbage collection
      //Declare quiescent state
      //Collect garbage created by prior transactions
      trans.mainte();
    }
  }

  return nullptr;
}

int 
main(int argc, char *argv[]) try
{
  chkArg(argc, argv);

  CicadaResult rsob[THREAD_NUM];
  CicadaResult &rsroot = rsob[0];
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
    rsroot.addLocalAllCicadaResult(rsob[i]);
  }

  rsroot.extime_ = EXTIME;
  rsroot.displayAllCicadaResult();

  return 0;
} catch (bad_alloc) {
  ERR;
}
