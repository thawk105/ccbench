#include <ctype.h>  // isdigit,
#include <iostream>
#include <pthread.h>
#include <string.h> // strlen,
#include <string> // string
#include <sys/syscall.h>  // syscall(SYS_gettid),
#include <sys/types.h>  // syscall(SYS_gettid),
#include <time.h>
#include <unistd.h> // syscall(SYS_gettid),

#define GLOBAL_VALUE_DEFINE

#include "../include/debug.hpp"
#include "../include/int64byte.hpp"
#include "../include/tsc.hpp"
#include "../include/zipf.hpp"

#include "include/atomic_tool.hpp"
#include "include/common.hpp"
#include "include/result.hpp"
#include "include/transaction.hpp"

using namespace std;

extern void chkArg(const int argc, char *argv[]);
extern bool chkClkSpan(uint64_t &start, uint64_t &stop, uint64_t threshold);
extern void displayDB();
extern void displayPRO();
extern void makeDB();
extern void makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd);
extern void makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd, FastZipf &zipf);
extern void setThreadAffinity(int myid);
extern void waitForReadyOfAllThread();

bool
chkEpochLoaded()
{
  uint64_t_64byte nowepo = loadAcquireGE();
  for (unsigned int i = 1; i < THREAD_NUM; ++i) {
    if (__atomic_load_n(&(ThLocalEpoch[i].obj), __ATOMIC_ACQUIRE) != nowepo.obj) return false;
  }

  return true;
}

static void *
epoch_worker(void *arg)
{
  int *myid = (int *)arg;
  uint64_t EpochTimerStart, EpochTimerStop;
  Result rsobject;

#ifdef Linux
  setThreadAffinity(*myid);
#endif

  waitForReadyOfAllThread();

  rsobject.Bgn = rdtsc();
  EpochTimerStart = rdtsc();
  for (;;) {
    usleep(1);
    rsobject.End = rdtsc();
    if (chkClkSpan(rsobject.Bgn, rsobject.End, EXTIME * 1000 * 1000 * CLOCK_PER_US)) {
      rsobject.Finish.store(true, memory_order_release);
      return nullptr;
    }

    //Epoch Control
    EpochTimerStop = rdtsc();
    //chkEpochLoaded は最新のグローバルエポックを
    //全てのワーカースレッドが読み込んだか確認する．
    if (chkClkSpan(EpochTimerStart, EpochTimerStop, EPOCH_TIME * CLOCK_PER_US * 1000) && chkEpochLoaded()) {
      atomicAddGE();
      EpochTimerStart = EpochTimerStop;
    }
    //----------
  }
  
  return nullptr;
}

static void *
worker(void *arg)
{
  int *myid = (int *) arg;
  Xoroshiro128Plus rnd;
  rnd.init();
  Procedure pro[MAX_OPE];
  int locknum = *myid + 2;
  // 0 : None
  // 1 : Acquired
  // 2 : SuccessorLeaving
  // 3 : th num 1
  // 4 : th num 2
  // ..., th num 0 is leader thread.
  TxExecutor trans(*myid, &rnd, locknum);
  FastZipf zipf(&rnd, ZIPF_SKEW, TUPLE_NUM);

#ifdef Linux
  setThreadAffinity(*myid);
#endif // Linux

  waitForReadyOfAllThread();
  
  try {
    //start work (transaction)
    for (;;) {
      if (YCSB)
        makeProcedure(pro, rnd, zipf);
      else
        makeProcedure(pro, rnd);

      asm volatile ("" ::: "memory");
RETRY:
      if (trans.rsob.Finish.load(memory_order_acquire)) {
        trans.rsob.sumUpAbortCounts();
        trans.rsob.sumUpCommitCounts();
#ifdef DEBUG
        trans.rsob.sumUpAbortByOperation();
        trans.rsob.sumUpAbortByValidation();
        trans.rsob.sumUpValidationFailureByWriteLock();
        trans.rsob.sumUpValidationFailureByTID();
#endif // DEBUG
        return nullptr;
      }

      trans.begin();
      for (unsigned int i = 0; i < MAX_OPE; ++i) {
        if (pro[i].ope == Ope::READ) {
          trans.read(pro[i].key);
        } else {
          if (RMW) {
            trans.read(pro[i].key);
            trans.write(pro[i].key);
          } else 
            trans.write(pro[i].key);
        }

        if (trans.status == TransactionStatus::aborted) {
          trans.abort();
#ifdef DEBUG
          ++trans.rsob.localAbortByOperation;
#endif // DEBUG
          goto RETRY;
        }
      }

      if (!(trans.commit())) {
        trans.abort();
#ifdef DEBUG
          ++trans.rsob.localAbortByValidation;
#endif // DEBUG
        goto RETRY;
      }

      trans.writePhase();
    }
  } catch (bad_alloc) {
    ERR;
  }

  return nullptr;
}

static pthread_t
threadCreate(int id)
{
  pthread_t t;
  int *myid;

  try {
    myid = new int;
  } catch (bad_alloc) {
    ERR;
  }
  *myid = id;

  if (*myid == 0) {
    if (pthread_create(&t, nullptr, epoch_worker, (void *)myid)) ERR;
  } else {
    if (pthread_create(&t, nullptr, worker, (void *)myid)) ERR;
  }

  return t;
}

int
main(int argc, char *argv[]) 
{
  Result rsobject;
  chkArg(argc, argv);
  makeDB();

  pthread_t thread[THREAD_NUM];

  for (unsigned int i = 0; i < THREAD_NUM; ++i) {
    thread[i] = threadCreate(i);
  }
  
  for (unsigned int i = 0; i < THREAD_NUM; ++i) {
    pthread_join(thread[i], nullptr);
  }

  rsobject.displayTPS();
  rsobject.displayAbortRate();
#ifdef DEBUG
  //rsobject.displayAbortByOperationRate();
  rsobject.displayAbortByValidationRate();
  rsobject.displayValidationFailureByWriteLockRate();
  rsobject.displayValidationFailureByTIDRate();
#endif // DEBUG

  return 0;
}
