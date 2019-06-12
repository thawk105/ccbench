
#include <ctype.h>
#include <pthread.h>
#include <string.h>
#include <sched.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>

#define GLOBAL_VALUE_DEFINE

#include "include/common.hpp"
#include "include/result.hpp"
#include "include/transaction.hpp"

#include "../include/cpu.hpp"
#include "../include/debug.hpp"
#include "../include/masstree_wrapper.hpp"
#include "../include/random.hpp"
#include "../include/result.hpp"
#include "../include/tsc.hpp"
#include "../include/zipf.hpp"

extern void chkArg(const int argc, char *argv[]);
extern bool chkClkSpan(const uint64_t start, const uint64_t stop, const uint64_t threshold);
extern void displayDB();
extern void displayPRO();
extern void makeDB();
extern void makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd);
extern void makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd, FastZipf &zipf);
extern void ReadyAndWaitForReadyOfAllThread(std::atomic<size_t> &running, const size_t thnm);
extern void waitForReadyOfAllThread(std::atomic<size_t> &running, const size_t thnm);
extern void sleepMs(size_t ms);

static void *
worker(void *arg)
{
  TicTocResult &res = *(TicTocResult *)(arg);
  Xoroshiro128Plus rnd;
  rnd.init();
  Procedure pro[MAX_OPE];
  TxExecutor trans(res.thid, &res);
  FastZipf zipf(&rnd, ZIPF_SKEW, TUPLE_NUM);

#if MASSTREE_USE
  MasstreeWrapper<Tuple>::thread_init(int(res.thid));
#endif

  setThreadAffinity(res.thid);
  //printf("Thread #%d: on CPU %d\n", *myid, sched_getcpu());
  //printf("sysconf(_SC_NPROCESSORS_CONF) %d\n", sysconf(_SC_NPROCESSORS_CONF));
  ReadyAndWaitForReadyOfAllThread(Running, THREAD_NUM);
  
  for (;;) {
    if (YCSB)
      makeProcedure(pro, rnd, zipf);
    else
      makeProcedure(pro, rnd);
RETRY:
    trans.tbegin();

    if (res.Finish.load(std::memory_order_acquire))
      return nullptr;

    for (unsigned int i = 0; i < MAX_OPE; ++i) {
      if (pro[i].ope == Ope::READ) {
        trans.tread(pro[i].key);
        if (trans.status == TransactionStatus::aborted) {
          trans.abort();
          ++res.localAbortCounts;
          goto RETRY;
        }
      } 
      else if (pro[i].ope == Ope::WRITE) {
        if (RMW) {
          trans.tread(pro[i].key);
          trans.twrite(pro[i].key);
        }
        else
          trans.twrite(pro[i].key);
      }
      else
        ERR;
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

  return nullptr;
}

int 
main(int argc, char *argv[]) try
{
  chkArg(argc, argv);
  makeDB();
  
  TicTocResult rsob[THREAD_NUM];
  TicTocResult &rsroot = rsob[0];
  pthread_t thread[THREAD_NUM];
  for (unsigned int i = 0; i < THREAD_NUM; ++i) {
    int ret;
    rsob[i].thid = i;
    ret = pthread_create(&thread[i], NULL, worker, (void *)(&rsob[i]));
    if (ret) ERR;
  }

  waitForReadyOfAllThread(Running, THREAD_NUM);
  for (size_t i = 0; i < EXTIME; ++i) {
    sleepMs(1000);
  }
  TicTocResult::Finish.store(true, std::memory_order_release);

  for (unsigned int i = 0; i < THREAD_NUM; ++i) {
    pthread_join(thread[i], nullptr);
    rsroot.add_local_all_TicTocResult(rsob[i]);
  }

  rsroot.extime = EXTIME;
  rsroot.display_all_TicTocResult();

  return 0;
} catch (bad_alloc) {
  ERR;
}

