
#include <ctype.h>  //isdigit, 
#include <pthread.h>
#include <string.h> //strlen,
#include <sys/types.h>  //syscall(SYS_gettid),
#include <sys/syscall.h>  //syscall(SYS_gettid),  
#include <unistd.h> //syscall(SYS_gettid), 

#include <iostream>
#include <string> //string
#include <thread>

#define GLOBAL_VALUE_DEFINE

#include "include/common.hpp"
#include "include/transaction.hpp"

#include "../include/cpu.hpp"
#include "../include/debug.hpp"
#include "../include/int64byte.hpp"
#include "../include/random.hpp"
#include "../include/result.hpp"
#include "../include/tsc.hpp"
#include "../include/util.hpp"
#include "../include/zipf.hpp"

extern void chkArg(const int argc, const char *argv[]);
extern bool chkClkSpan(const uint64_t start, const uint64_t stop, const uint64_t threshold);
extern void displayDB();
extern void displayPRO();
extern void makeDB();
extern void makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd);
extern void makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd, FastZipf &zipf);
extern void waitForReadyOfAllThread(std::atomic<unsigned int> &running, const unsigned int thnum);

static void *
worker(void *arg)
{
  Result &res = *(Result *)(arg);
  Xoroshiro128Plus rnd;
  rnd.init();
  Procedure pro[MAX_OPE];
  TxExecutor trans(res.thid);
  FastZipf zipf(&rnd, ZIPF_SKEW, TUPLE_NUM);

#ifdef Linux
  setThreadAffinity(res.thid);
  //printf("Thread #%d: on CPU %d\n", *myid, sched_getcpu());
  //printf("sysconf(_SC_NPROCESSORS_CONF) %ld\n", sysconf(_SC_NPROCESSORS_CONF));
#endif // Linux

  waitForReadyOfAllThread(Running, THREAD_NUM);
  
  if (res.thid == 0) res.bgn = rdtscp();

  try {
    //start work (transaction)
    for (;;) {
      if (YCSB) 
        makeProcedure(pro, rnd, zipf);
      else
        makeProcedure(pro, rnd);
RETRY:
      trans.tbegin();

      //End judgment
      if (res.thid == 0) {
        res.end = rdtscp();
        if (chkClkSpan(res.bgn, res.end, EXTIME * 1000 * 1000 * CLOCKS_PER_US)) {
          res.Finish.store(true, std::memory_order_release);
          return nullptr;
        }
      } else {
        if (res.Finish.load(std::memory_order_acquire))
          return nullptr;
      }
      //-----
      
      for (unsigned int i = 0; i < MAX_OPE; ++i) {
        if (pro[i].ope == Ope::READ) {
          trans.tread(pro[i].key);
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

        if (trans.status == TransactionStatus::aborted) {
          trans.abort();
          ++res.localAbortCounts;
          goto RETRY;
        }
      }

      //commit - write phase
      trans.commit();
      ++res.localCommitCounts;
    }
  } catch (bad_alloc) {
    ERR;
  }

  return nullptr;
}

int
main(const int argc, const char *argv[])
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
    ret = pthread_create(&thread[i], NULL, worker, (void *)(&rsob[i]));
    if (ret) ERR;
  }

  for (unsigned int i = 0; i < THREAD_NUM; i++) {
    pthread_join(thread[i], nullptr);
    rsroot.add_localAllResult(rsob[i]);
  }

  rsroot.display_AllResult(CLOCKS_PER_US);

  return 0;
}
