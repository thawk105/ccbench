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
#include "include/atomic_tool.hh"
#include "include/common.hh"
#include "include/result.hh"
#include "include/transaction.hh"

#include "../include/cpu.hh"
#include "../include/debug.hh"
#include "../include/fileio.hh"
#include "../include/masstree_wrapper.hh"
#include "../include/random.hh"
#include "../include/tsc.hh"
#include "../include/util.hh"
#include "../include/zipf.hh"

using namespace std;

extern void chkArg(const int argc, char *argv[]);
extern bool chkClkSpan(const uint64_t start, const uint64_t stop, const uint64_t threshold);
extern bool chkEpochLoaded();
extern void displayDB();
extern void displayPRO();
extern void genLogFile(std::string &logpath, const int thid);
extern void makeDB();
extern void makeProcedure(std::vector<Procedure>& pro, Xoroshiro128Plus &rnd, FastZipf &zipf, size_t tuple_num, size_t max_ope, size_t rratio, bool rmw, bool ycsb);
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

  SiloResult &res = *(SiloResult *)(arg);
  uint64_t epochTimerStart, epochTimerStop;
  SiloResult rsobject;

  setThreadAffinity(res.thid_);
  //printf("Thread #%d: on CPU %d\n", res.thid_, sched_getcpu());
  ReadyAndWaitForReadyOfAllThread(Running, THREAD_NUM);

  epochTimerStart = rdtscp();

  for (;;) {
    if (SiloResult::Finish_.load(memory_order_acquire))
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
  SiloResult &res = *(SiloResult *)(arg);
  Xoroshiro128Plus rnd;
  rnd.init();
  TxnExecutor trans(res.thid_, (SiloResult*)arg);
  FastZipf zipf(&rnd, ZIPF_SKEW, TUPLE_NUM);
  
  //std::string logpath;
  //genLogFile(logpath, res.thid_);
  //trans.logfile.open(logpath, O_TRUNC | O_WRONLY, 0644);
  //trans.logfile.ftruncate(10^9);

#if MASSTREE_USE
  MasstreeWrapper<Tuple>::thread_init(int(res.thid_));
#endif

  setThreadAffinity(res.thid_);
  //printf("Thread #%d: on CPU %d\n", res.thid_, sched_getcpu());
  //printf("sysconf(_SC_NPROCESSORS_CONF) %d\n", sysconf(_SC_NPROCESSORS_CONF));
  ReadyAndWaitForReadyOfAllThread(Running, THREAD_NUM);
  
  //start work(transaction)
  for (;;) {
    makeProcedure(trans.pro_set_, rnd, zipf, TUPLE_NUM, MAX_OPE, RRATIO, RMW, YCSB);
RETRY:
    trans.tbegin();
    if (SiloResult::Finish_.load(memory_order_acquire))
      return nullptr;

    //Read phase
#if ADD_ANALYSIS
    uint64_t start, stop;
    start = rdtscp();
#endif
    for (auto itr = trans.pro_set_.begin(); itr != trans.pro_set_.end(); ++itr) {
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
    }
#if ADD_ANALYSIS
    stop = rdtscp();
    res.local_read_latency_ += stop - start;
#endif
    
    //Validation phase
#if ADD_ANALYSIS
    start = rdtscp();
#endif
    bool varesult = trans.validationPhase();
#if ADD_ANALYSIS
    stop = rdtscp();
    res.local_vali_latency_ += stop - start;
#endif
    if (varesult) {
      trans.writePhase();
      ++res.local_commit_counts_;
    } else {
      trans.abort();
      ++res.local_abort_counts_;
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

  SiloResult rsob[THREAD_NUM];
  SiloResult &rsroot = rsob[0];
  pthread_t thread[THREAD_NUM];
  for (unsigned int i = 0; i < THREAD_NUM; ++i) {
    int ret;
    rsob[i].thid_ = i;
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
  SiloResult::Finish_.store(true, std::memory_order_release);

  for (unsigned int i = 0; i < THREAD_NUM; ++i) {
    pthread_join(thread[i], NULL);
    rsroot.addLocalAllSiloResult(rsob[i]);
  }

  rsroot.extime_ = EXTIME;
  rsroot.displayAllSiloResult();

  return 0;
} catch (bad_alloc) {
  ERR;
}

