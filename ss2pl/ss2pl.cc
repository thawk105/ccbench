
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
#include "../include/fence.hpp"
#include "../include/int64byte.hpp"
#include "../include/masstree_wrapper.hpp"
#include "../include/procedure.hpp"
#include "../include/random.hpp"
#include "../include/result.hpp"
#include "../include/tsc.hpp"
#include "../include/util.hpp"
#include "../include/zipf.hpp"

extern void chkArg(const int argc, const char *argv[]);
extern bool chkClkSpan(const uint64_t start, const uint64_t stop, const uint64_t threshold);
extern void display_procedure_vector(std::vector<Procedure>& pro);
extern void displayDB();
extern void displayPRO();
extern void makeDB();
extern void makeProcedure(std::vector<Procedure>& pro, Xoroshiro128Plus &rnd, size_t& tuple_num, size_t& max_ope, size_t& rratio);
extern void makeProcedure(std::vector<Procedure>& pro, Xoroshiro128Plus &rnd, FastZipf &zipf, size_t& tuple_num, size_t& max_ope, size_t& rratio);
extern void ReadyAndWaitForReadyOfAllThread(std::atomic<size_t> &running, const size_t thnm);
extern void waitForReadyOfAllThread(std::atomic<size_t> &running, const size_t thnm);
extern void sleepMs(size_t ms);

static void *
worker(void *arg)
{
  Result &res = *(Result *)(arg);
  Xoroshiro128Plus rnd;
  rnd.init();
  TxExecutor trans(res.thid);
  FastZipf zipf(&rnd, ZIPF_SKEW, TUPLE_NUM);
  vector<Procedure> proSet;
  proSet.reserve(MAX_OPE);

#if MASSTREE_USE
  MasstreeWrapper<Tuple>::thread_init(int(res.thid));
#endif

#ifdef Linux
  setThreadAffinity(res.thid);
  //printf("Thread #%d: on CPU %d\n", *myid, sched_getcpu());
  //printf("sysconf(_SC_NPROCESSORS_CONF) %ld\n", sysconf(_SC_NPROCESSORS_CONF));
#endif // Linux

  ReadyAndWaitForReadyOfAllThread(Running, THREAD_NUM);
  
  try {
    //start work (transaction)
    for (;;) {
      if (YCSB) 
        makeProcedure(proSet, rnd, zipf, TUPLE_NUM, MAX_OPE, RRATIO);
      else
        makeProcedure(proSet, rnd, TUPLE_NUM, MAX_OPE, RRATIO);
#if KEY_SORT
      std::sort(proSet.begin(), proSet.end());
#endif

RETRY:
      trans.tbegin();

      //End judgment
      if (Result::Finish.load(std::memory_order_acquire))
          return nullptr;
      //-----
   
      for (unsigned int i = 0; i < MAX_OPE; ++i) {
        if (proSet[i].ope == Ope::READ) {
          trans.tread(proSet[i].key);
        } 
        else if (proSet[i].ope == Ope::WRITE) {
          if (RMW) {
            trans.tread(proSet[i].key);
            trans.twrite(proSet[i].key);
          }
          else
            trans.twrite(proSet[i].key);
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

  waitForReadyOfAllThread(Running, THREAD_NUM);
  for (size_t i = 0; i < EXTIME; ++i) {
    sleepMs(1000);
  }
  Result::Finish.store(true, std::memory_order_release);

  for (unsigned int i = 0; i < THREAD_NUM; ++i) {
    pthread_join(thread[i], nullptr);
    rsroot.add_localAllResult(rsob[i]);
  }

  rsroot.extime = EXTIME;
  rsroot.display_AllResult();

  //displayDB();

  return 0;
}
