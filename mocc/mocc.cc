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
#include "../include/masstree_wrapper.hpp"
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
extern void makeProcedure(std::vector<Procedure>& pro, Xoroshiro128Plus &rnd, size_t& tuple_num, size_t& max_ope, size_t& rratio);
extern void makeProcedure(std::vector<Procedure>& pro, Xoroshiro128Plus &rnd, FastZipf &zipf, size_t& tuple_num, size_t& max_ope, size_t& rratio);
extern void ReadyAndWaitForReadyOfAllThread(std::atomic<size_t> &running, size_t thnm);
extern void waitForReadyOfAllThread(std::atomic<size_t> &running, size_t thnm);
extern void sleepMs(size_t ms);

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

  ReadyAndWaitForReadyOfAllThread(Running, THREAD_NUM);

  res.bgn = rdtscp();
  epochTimerStart = rdtscp();
  for (;;) {
    if (Result::Finish.load(memory_order_acquire))
      return nullptr;

    usleep(1);

    //Epoch Control
    epochTimerStop = rdtscp();
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
  int locknum = res.thid + 2;
  // 0 : None
  // 1 : Acquired
  // 2 : SuccessorLeaving
  // 3 : th num 1
  // 4 : th num 2
  // ..., th num 0 is leader thread.
  TxExecutor trans(res.thid, &rnd, locknum);
  FastZipf zipf(&rnd, ZIPF_SKEW, TUPLE_NUM);

#if MASSTREE_USE
  MasstreeWrapper<Tuple>::thread_init(int(res.thid));
#endif

#ifdef Linux
  setThreadAffinity(res.thid);
#endif // Linux

  ReadyAndWaitForReadyOfAllThread(Running, THREAD_NUM);
  
  //start work (transaction)
  for (;;) {
    if (YCSB)
      makeProcedure(trans.proSet, rnd, zipf, TUPLE_NUM, MAX_OPE, RRATIO);
    else
      makeProcedure(trans.proSet, rnd, TUPLE_NUM, MAX_OPE, RRATIO);
#if KEY_SORT
    sort(trans.proSet.begin(), trans.proSet.end());
#endif

RETRY:
    if (Result::Finish.load(memory_order_acquire))
      return nullptr;

    trans.begin();
    for (unsigned int i = 0; i < MAX_OPE; ++i) {
      if (trans.proSet[i].ope == Ope::READ) {
        trans.read(trans.proSet[i].key);
      } else {
        if (RMW) {
          trans.read(trans.proSet[i].key);
          trans.write(trans.proSet[i].key);
        } else 
          trans.write(trans.proSet[i].key);
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

  return nullptr;
}

int
main(int argc, char *argv[])  try
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
 
 waitForReadyOfAllThread(Running, THREAD_NUM);
 for (size_t i = 0; i < EXTIME; ++i) {
   sleepMs(1000);
 }
 Result::Finish.store(true, std::memory_order_release);

  for (unsigned int i = 0; i < THREAD_NUM; ++i) {
    pthread_join(thread[i], nullptr);
    rsroot.add_localAllMoccResult(rsob[i]);
  }

  rsroot.extime = EXTIME;
  rsroot.display_AllMoccResult();

  return 0;
} catch (bad_alloc) {
  ERR;
}
