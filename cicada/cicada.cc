#include <algorithm>
#include <cctype>
#include <cstdint>
#include <ctype.h>
#include <pthread.h>
#include <random>
#include <string.h>
#include <sched.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdlib.h>
#include <thread>
#include <unistd.h>

#define GLOBAL_VALUE_DEFINE
#include "include/common.hpp"
#include "include/procedure.hpp"
#include "include/result.hpp"
#include "include/transaction.hpp"

#include "../include/debug.hpp"
#include "../include/int64byte.hpp"
#include "../include/random.hpp"
#include "../include/zipf.hpp"

using namespace std;

extern void chkArg(const int argc, char *argv[]);
extern bool chkClkSpan(uint64_t &start, uint64_t &stop, uint64_t threshold);
extern bool chkSpan(struct timeval &start, struct timeval &stop, long threshold);
extern void makeDB(uint64_t *initial_wts);
extern void makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd);
extern void makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd, FastZipf &zipf);
extern void setThreadAffinity(int myid);
extern void waitForReadyOfAllThread();

static void *
manager_worker(void *arg)
{
  int *myid = (int *)arg;
  TimeStamp tmp;

  uint64_t initial_wts;
  makeDB(&initial_wts);
  MinWts.store(initial_wts + 2, memory_order_release);
  Result rsobject;

  setThreadAffinity(*myid);
  //printf("Thread #%d: on CPU %d\n", *myid, sched_getcpu());
  waitForReadyOfAllThread();
  while (FirstAllocateTimestamp.load(memory_order_acquire) != THREAD_NUM - 1) {}

  rsobject.Bgn = rdtsc();
  // leader work
  for(;;) {
    rsobject.End = rdtsc();
    if (chkClkSpan(rsobject.Bgn, rsobject.End, EXTIME * 1000 * 1000 * CLOCK_PER_US)) {
      rsobject.Finish.store(true, std::memory_order_release);
      return nullptr;
    }

    bool gc_update = true;
    for (unsigned int i = 1; i < THREAD_NUM; ++i) {
    //check all thread's flag raising
      if (__atomic_load_n(&(GCFlag[i].obj), __ATOMIC_ACQUIRE) == 0) {
        usleep(1);
        gc_update = false;
        break;
      }
    }
    if (gc_update) {
      uint64_t minw = __atomic_load_n(&(ThreadWtsArray[1].obj), __ATOMIC_ACQUIRE);
      uint64_t minr;
      if (GROUP_COMMIT == 0) {
        minr = __atomic_load_n(&(ThreadRtsArray[1].obj), __ATOMIC_ACQUIRE);
      }
      else {
        minr = __atomic_load_n(&(ThreadRtsArrayForGroup[1].obj), __ATOMIC_ACQUIRE);
      }

      for (unsigned int i = 1; i < THREAD_NUM; ++i) {
        uint64_t tmp = __atomic_load_n(&(ThreadWtsArray[i].obj), __ATOMIC_ACQUIRE);
        if (minw > tmp) minw = tmp;
        if (GROUP_COMMIT == 0) {
          tmp = __atomic_load_n(&(ThreadRtsArray[i].obj), __ATOMIC_ACQUIRE);
          if (minr > tmp) minr = tmp;
        }
        else {
          tmp = __atomic_load_n(&(ThreadRtsArrayForGroup[i].obj), __ATOMIC_ACQUIRE);
          if (minr > tmp) minr = tmp;
        }
      }
      MinWts.store(minw, memory_order_release);
      MinRts.store(minr, memory_order_release);

      // downgrade gc flag
      for (unsigned int i = 1; i < THREAD_NUM; ++i) {
        __atomic_store_n(&(GCFlag[i].obj), 0, __ATOMIC_RELEASE);
        __atomic_store_n(&(GCExecuteFlag[i].obj), 1, __ATOMIC_RELEASE);
      }
    }
  }

  return nullptr;
}

static void *
worker(void *arg)
{
  int *myid = (int *)arg;
  Xoroshiro128Plus rnd;
  rnd.init();
  Procedure pro[MAX_OPE];
  TxExecutor trans(*myid);
  FastZipf zipf(&rnd, ZIPF_SKEW, TUPLE_NUM);

  setThreadAffinity(*myid);
  //printf("Thread #%d: on CPU %d\n", *myid, sched_getcpu());
  //printf("sysconf(_SC_NPROCESSORS_CONF) %d\n", sysconf(_SC_NPROCESSORS_CONF));
  waitForReadyOfAllThread();

  //printf("%s\n", trans.writeVal);

  try {
    //start work(transaction)
    for (;;) {
      if (YCSB) 
        makeProcedure(pro, rnd, zipf);
      else 
        makeProcedure(pro, rnd);

      asm volatile ("" ::: "memory");
RETRY:
      if (trans.rsobject.Finish.load(std::memory_order_acquire)) {
        trans.rsobject.sumUpAbortCounts();
        trans.rsobject.sumUpCommitCounts();
        trans.rsobject.sumUpGCCounts();
        //trans.rsobject.displayLocalCommitCounts();
        return nullptr;
      }

      trans.tbegin(pro[0].ronly);

      //Read phase
      //Search versions
      for (unsigned int i = 0; i < MAX_OPE; ++i) {
        if (pro[i].ope == Ope::READ) {
          trans.tread(pro[i].key);
        } else {
          if (RMW) {
            trans.tread(pro[i].key);
            trans.twrite(pro[i].key);
          } else 
            trans.twrite(pro[i].key);
        }

        if (trans.status == TransactionStatus::abort) {
          trans.earlyAbort();
          goto RETRY;
        }
      }

      //read onlyトランザクションはread setを集めず、validationもしない。
      //write phaseはログを取り仮バージョンのコミットを行う．これをスキップできる．
      if (trans.ronly) {
        ++trans.rsobject.localCommitCounts;
        ++trans.continuingCommit;
        continue;
      }

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

  if (id == 0) {
    if (pthread_create(&t, nullptr, manager_worker, (void *)myid)) ERR;
    return t;
  }
  else {
    if (pthread_create(&t, nullptr, worker, (void *)myid)) ERR;
    return t;
  }
}

int 
main(int argc, char *argv[]) 
{
  Result rsobject;
  chkArg(argc, argv);

  pthread_t thread[THREAD_NUM];

  for (unsigned int i = 0; i < THREAD_NUM; i++) {
    thread[i] = threadCreate(i);
  }

  for (unsigned int i = 0; i < THREAD_NUM; i++) {
    pthread_join(thread[i], nullptr);
  }

  rsobject.displayTPS();
  //rsobject.displayCommitCounts();
  //rsobject.displayAbortCounts();
  //rsobject.displayAbortRate();
  //rsobject.displayGCCounts();

  return 0;
}
