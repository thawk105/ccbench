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

#include "include/atomic_tool.hpp"
#include "include/common.hpp"
#include "include/result.hpp"
#include "include/transaction.hpp"

#include "../include/cpu.hpp"
#include "../include/debug.hpp"
#include "../include/int64byte.hpp"
#include "../include/tsc.hpp"
#include "../include/zipf.hpp"

using namespace std;

extern void chkArg(const int argc, char *argv[]);
extern bool chkClkSpan(const uint64_t start, const uint64_t stop, const uint64_t threshold);
extern void displayDB();
extern void displayLockedTuple();
extern void displayPRO();
extern void makeDB();
extern void makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd);
extern void makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd, FastZipf &zipf);
extern void waitForReadyOfAllThread(std::atomic<unsigned int> &running, const unsigned int thnum);

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
manager_worker(void *arg)
{
  MoccResult &res = *(MoccResult *)(arg);
  uint64_t epochTimerStart, epochTimerStop;

#ifdef Linux
  setThreadAffinity(res.thid);
#endif

  waitForReadyOfAllThread(Running, THREAD_NUM);

  res.bgn = rdtsc();
  epochTimerStart = rdtsc();
  for (;;) {
    usleep(1);
    res.end = rdtsc();
    if (chkClkSpan(res.bgn, res.end, EXTIME * 1000 * 1000 * CLOCKS_PER_US)) {
      res.Finish.store(true, memory_order_release);
      return nullptr;
    }

    //Epoch Control
    epochTimerStop = rdtsc();
    //chkEpochLoaded は最新のグローバルエポックを
    //全てのワーカースレッドが読み込んだか確認する．
    if (chkClkSpan(epochTimerStart, epochTimerStop, EPOCH_TIME * CLOCKS_PER_US * 1000) && chkEpochLoaded()) {
      atomicAddGE();
      epochTimerStart = epochTimerStop;
    }
    //----------
  }
  
  return nullptr;
}

static void *
worker(void *arg)
{
  MoccResult &res = *(MoccResult *)(arg);
  Xoroshiro128Plus rnd;
  rnd.init();
  Procedure pro[MAX_OPE];
  int locknum = res.thid + 2;
  // 0 : None
  // 1 : Acquired
  // 2 : SuccessorLeaving
  // 3 : th num 1
  // 4 : th num 2
  // ..., th num 0 is leader thread.
  TxExecutor trans(res.thid, &rnd, locknum);
  FastZipf zipf(&rnd, ZIPF_SKEW, TUPLE_NUM);

#ifdef Linux
  setThreadAffinity(res.thid);
#endif // Linux

  waitForReadyOfAllThread(Running, THREAD_NUM);
  
  try {
    //start work (transaction)
    for (;;) {
      if (YCSB)
        makeProcedure(pro, rnd, zipf);
      else
        makeProcedure(pro, rnd);

      asm volatile ("" ::: "memory");
RETRY:
      if (res.Finish.load(memory_order_acquire))
        return nullptr;

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
          ++res.localAbortByOperation;
          ++res.localAbortCounts;
          goto RETRY;
        }
      }

      if (!(trans.commit(res))) {
        trans.abort();
        ++res.localAbortByValidation;
        ++res.localAbortCounts;
        goto RETRY;
      }

      trans.writePhase();
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
  makeDB();

  MoccResult rsob[THREAD_NUM];
  MoccResult &rsroot = rsob[0];
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
  
  for (unsigned int i = 0; i < THREAD_NUM; ++i) {
    pthread_join(thread[i], nullptr);
    rsroot.add_localAllMoccResult(rsob[i]);
  }

  rsroot.display_AllMoccResult(CLOCKS_PER_US);

  return 0;
}
