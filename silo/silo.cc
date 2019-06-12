#include <cctype>
#include <ctype.h>
#include <algorithm>
#include <string.h>
#include <sched.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/time.h>

#define GLOBAL_VALUE_DEFINE
#include "include/atomic_tool.hpp"
#include "include/common.hpp"
#include "include/transaction.hpp"

#include "../include/cpu.hpp"
#include "../include/debug.hpp"
#include "../include/fileio.hpp"
#include "../include/masstree_wrapper.hpp"
#include "../include/random.hpp"
#include "../include/result.hpp"
#include "../include/tsc.hpp"
#include "../include/util.hpp"
#include "../include/zipf.hpp"

using namespace std;

extern void chkArg(const int argc, char *argv[]);
extern bool chkClkSpan(const uint64_t start, const uint64_t stop, const uint64_t threshold);
extern bool chkEpochLoaded();
extern void displayDB();
extern void displayPRO();
extern void genLogFile(std::string &logpath, const int thid);
extern void makeDB();
extern void makeProcedure(std::vector<Procedure>& pro, Xoroshiro128Plus &rnd, size_t& tuple_num, size_t& max_ope, size_t& rratio);
extern void makeProcedure(std::vector<Procedure>& pro, Xoroshiro128Plus &rnd, FastZipf &zipf, size_t& tuple_num, size_t& max_ope, size_t& rratio);
extern void ReadyAndWaitForReadyOfAllThread(std::atomic<size_t> &running, size_t thnm);
extern void waitForReadyOfAllThread(std::atomic<size_t> &running, size_t thnm);
extern void sleepMs(size_t ms);

static void *
epoch_worker(void *arg)
{
  /* 1. 40msごとに global epoch を必要に応じてインクリメントする
   * 2. 十分条件
   * 全ての worker が最新の epoch を読み込んでいる。
   * */

  Result &res = *(Result *)(arg);
  uint64_t epochTimerStart, epochTimerStop;
  Result rsobject;

  setThreadAffinity(res.thid);
  //printf("Thread #%d: on CPU %d\n", res.thid, sched_getcpu());
  ReadyAndWaitForReadyOfAllThread(Running, THREAD_NUM);

  epochTimerStart = rdtscp();

  for (;;) {
    if (Result::Finish.load(memory_order_acquire))
      return nullptr;

    usleep(1);

    epochTimerStop = rdtscp();
    //chkEpochLoaded は最新のグローバルエポックを
    //全てのワーカースレッドが読み込んだか確認する．
    if (chkClkSpan(epochTimerStart, epochTimerStop, EPOCH_TIME * CLOCKS_PER_US * 1000) && chkEpochLoaded()) {
      atomicAddGE();
      epochTimerStart = epochTimerStop;
    }
  }
  //----------

  return nullptr;
}

static void *
worker(void *arg)
{
  Result &res = *(Result *)(arg);
  Xoroshiro128Plus rnd;
  rnd.init();
  TxnExecutor trans(res.thid);
  FastZipf zipf(&rnd, ZIPF_SKEW, TUPLE_NUM);
  
  //std::string logpath;
  //genLogFile(logpath, res.thid);
  //trans.logfile.open(logpath, O_TRUNC | O_WRONLY, 0644);
  //trans.logfile.ftruncate(10^9);

#if MASSTREE_USE
  MasstreeWrapper<Tuple>::thread_init(int(res.thid));
#endif

  setThreadAffinity(res.thid);
  //printf("Thread #%d: on CPU %d\n", res.thid, sched_getcpu());
  //printf("sysconf(_SC_NPROCESSORS_CONF) %d\n", sysconf(_SC_NPROCESSORS_CONF));
  ReadyAndWaitForReadyOfAllThread(Running, THREAD_NUM);
  
  //start work(transaction)
  for (;;) {
    if (YCSB)
      makeProcedure(trans.proSet, rnd, zipf, TUPLE_NUM, MAX_OPE, RRATIO);
    else
      makeProcedure(trans.proSet, rnd, TUPLE_NUM, MAX_OPE, RRATIO);

    asm volatile ("" ::: "memory");
RETRY:
    trans.tbegin();
    if (Result::Finish.load(memory_order_acquire))
      return nullptr;

    //Read phase
    for (unsigned int i = 0; i < MAX_OPE; ++i) {
      if (trans.proSet[i].ope == Ope::READ) {
          trans.tread(trans.proSet[i].key);
      }
      else {
        if (RMW) {
          trans.tread(trans.proSet[i].key);
          trans.twrite(trans.proSet[i].key);
        }
        else
          trans.twrite(trans.proSet[i].key);
      }
    }
    
    //Validation phase
    if (trans.validationPhase()) {
      trans.writePhase();
      ++res.localCommitCounts;
    } else {
      trans.abort();
      ++res.localAbortCounts;
      goto RETRY;
    }

  }

  return NULL;
}

int 
main(int argc, char *argv[]) try
{
  chkArg(argc, argv);
  makeDB();
  
  //displayDB();
  //displayPRO();

  Result rsob[THREAD_NUM];
  Result &rsroot = rsob[0];
  pthread_t thread[THREAD_NUM];
  for (unsigned int i = 0; i < THREAD_NUM; ++i) {
    int ret;
    rsob[i].thid = i;
    if (i == 0)
      ret = pthread_create(&thread[i], NULL, epoch_worker, (void *)(&rsob[i]));
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
    pthread_join(thread[i], NULL);
    rsroot.add_localAllResult(rsob[i]);
  }

  rsroot.extime = EXTIME;
  rsroot.display_AllResult();

  return 0;
} catch (bad_alloc) {
  ERR;
}

