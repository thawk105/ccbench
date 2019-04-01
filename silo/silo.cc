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
#include "include/procedure.hpp"
#include "include/transaction.hpp"

#include "../include/cpu.hpp"
#include "../include/debug.hpp"
#include "../include/fileio.hpp"
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
extern void makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd);
extern void makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd, FastZipf &zipf);
extern void waitForReadyOfAllThread(std::atomic<unsigned int> &running, const unsigned int thnum);

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
  waitForReadyOfAllThread(Running, THREAD_NUM);

  res.bgn = rdtsc();
  epochTimerStart = rdtsc();

  for (;;) {
    usleep(1);
    res.end = rdtsc();
    if (chkClkSpan(res.bgn, res.end, EXTIME * 1000 * 1000 * CLOCKS_PER_US)) {
      rsobject.Finish.store(true, std::memory_order_release);
      return nullptr;
    }

    epochTimerStop = rdtsc();
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
  Procedure pro[MAX_OPE];
  TxnExecutor trans(res.thid);
  FastZipf zipf(&rnd, ZIPF_SKEW, TUPLE_NUM);
  
  //std::string logpath;
  //genLogFile(logpath, res.thid);
  //trans.logfile.open(logpath, O_TRUNC | O_WRONLY, 0644);
  //trans.logfile.ftruncate(10^9);

  setThreadAffinity(res.thid);
  //printf("Thread #%d: on CPU %d\n", res.thid, sched_getcpu());
  //printf("sysconf(_SC_NPROCESSORS_CONF) %d\n", sysconf(_SC_NPROCESSORS_CONF));
  waitForReadyOfAllThread(Running, THREAD_NUM);
  
  try {
    //start work(transaction)
    for (;;) {
      if (YCSB)
        makeProcedure(pro, rnd, zipf);
      else
        makeProcedure(pro, rnd);

      asm volatile ("" ::: "memory");
RETRY:
      trans.tbegin();
      if (trans.rsobject.Finish.load(memory_order_acquire))
        return nullptr;

      //Read phase
      for (unsigned int i = 0; i < MAX_OPE; ++i) {
        if (pro[i].ope == Ope::TREAD) {
            trans.tread(pro[i].key);
        }
        else {
          if (RMW) {
            trans.tread(pro[i].key);
            trans.twrite(pro[i].key);
          }
          else
            trans.twrite(pro[i].key);
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
  } catch (bad_alloc) {
    ERR;
  }

  return NULL;
}

int 
main(int argc, char *argv[]) 
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

  for (unsigned int i = 0; i < THREAD_NUM; ++i) {
    pthread_join(thread[i], NULL);
    rsroot.add_localAllResult(rsob[i]);
  }


  //displayDB();
  rsroot.display_AllResult(CLOCKS_PER_US);

  return 0;
}
