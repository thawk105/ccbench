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

#include "../include/cpu.hpp"
#include "../include/debug.hpp"
#include "../include/int64byte.hpp"
#include "../include/random.hpp"
#include "../include/zipf.hpp"

using namespace std;

extern void chkArg(const int argc, char *argv[]);
extern bool chkClkSpan(const uint64_t start, const uint64_t stop, const uint64_t threshold);
extern bool chkSpan(struct timeval &start, struct timeval &stop, long threshold);
extern void makeDB(uint64_t *initial_wts);
extern void makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd);
extern void makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd, FastZipf &zipf);
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
  setThreadAffinity(res.thid);
  //printf("Thread #%d: on CPU %d\n", *myid, sched_getcpu());
#endif // Linux

  ReadyAndWaitForReadyOfAllThread(Running, THREAD_NUM);
  while (FirstAllocateTimestamp.load(memory_order_acquire) != THREAD_NUM - 1) {}

  // leader work
  for(;;) {
    if (res.Finish.load(std::memory_order_acquire))
      return nullptr;

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
  CicadaResult &res = *(CicadaResult *)(arg);
  Xoroshiro128Plus rnd;
  rnd.init();
  Procedure pro[MAX_OPE];
  TxExecutor trans(res.thid);
  FastZipf zipf(&rnd, ZIPF_SKEW, TUPLE_NUM);

#ifdef Linux
  setThreadAffinity(res.thid);
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

  try {
    //start work(transaction)
    for (;;) {
      if (YCSB) 
        makeProcedure(pro, rnd, zipf);
      else 
        makeProcedure(pro, rnd);

      asm volatile ("" ::: "memory");
RETRY:
      if (res.Finish.load(std::memory_order_acquire))
        return nullptr;

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
          ++res.localAbortCounts;
          goto RETRY;
        }
      }

      //read onlyトランザクションはread setを集めず、validationもしない。
      //write phaseはログを取り仮バージョンのコミットを行う．これをスキップできる．
      if (trans.ronly) {
        trans.mainte(res);
        ++trans.continuingCommit;
        ++res.localCommitCounts;
        continue;
      }

      //Validation phase
      if (!trans.validation()) {
        trans.abort();
        ++res.localAbortCounts;
        goto RETRY;
      }

      //Write phase
      trans.writePhase();

      //Maintenance
      //Schedule garbage collection
      //Declare quiescent state
      //Collect garbage created by prior transactions
      trans.mainte(res);
      ++trans.continuingCommit;
      ++res.localCommitCounts;
    }
  } catch (bad_alloc) {
    ERR;
  }

  return nullptr;
}

int 
main(int argc, char *argv[]) 
{
  chkArg(argc, argv);

  CicadaResult rsob[THREAD_NUM];
  CicadaResult &rsroot = rsob[0];
  pthread_t thread[THREAD_NUM];
  for (unsigned int i = 0; i < THREAD_NUM; ++i) {
    int ret;
    rsob[i].thid = i;
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
  Result::Finish.store(true, std::memory_order_release);


  for (unsigned int i = 0; i < THREAD_NUM; ++i) {
    pthread_join(thread[i], nullptr);
    rsroot.add_localAllCicadaResult(rsob[i]);
  }

  rsroot.extime = EXTIME;
  rsroot.display_AllCicadaResult();

  //if (system("cat /proc/meminfo | grep HugePages") != 0) ERR;
  return 0;
}
