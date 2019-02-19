
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

#include "../include/debug.hpp"
#include "../include/int64byte.hpp"
#include "../include/random.hpp"
#include "../include/tsc.hpp"
#include "../include/zipf.hpp"

#include "include/common.hpp"
#include "include/result.hpp"
#include "include/transaction.hpp"

using namespace std;

extern void chkArg(const int argc, const char *argv[]);
extern bool chkClkSpan(uint64_t &start, uint64_t &stop, uint64_t threshold);
extern void displayDB();
extern void displayPRO();
extern void makeDB();
extern void makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd);
extern void makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd, FastZipf &zipf);
extern void setThreadAffinity(int myid);
extern void waitForReadyOfAllThread();

static void *
worker(void *arg)
{

  const int *myid = (int *)arg;
  Xoroshiro128Plus rnd;
  rnd.init();
  Procedure pro[MAX_OPE];
  Transaction trans(*myid);
  Result rsobject;
  FastZipf zipf(&rnd, ZIPF_SKEW, TUPLE_NUM);

  setThreadAffinity(*myid);
  //printf("Thread #%d: on CPU %d\n", *myid, sched_getcpu());
  //printf("sysconf(_SC_NPROCESSORS_CONF) %ld\n", sysconf(_SC_NPROCESSORS_CONF));
  waitForReadyOfAllThread();
  
  if (*myid == 0) rsobject.Bgn = rdtsc();

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
      if (*myid == 0) {

        // finish judge
        rsobject.End = rdtsc();
        if (chkClkSpan(rsobject.Bgn, rsobject.End, EXTIME * 1000 * 1000 * CLOCK_PER_US)) {
          rsobject.Finish.store(true, std::memory_order_release);
          rsobject.sumUpCommitCounts();
          rsobject.sumUpAbortCounts();
          return nullptr;
        }
      } else {
        if (rsobject.Finish.load(std::memory_order_acquire)) {
          rsobject.sumUpCommitCounts();
          rsobject.sumUpAbortCounts();
          return nullptr;
        }
      }
      //-----
      
      for (unsigned int i = 0; i < MAX_OPE; ++i) {
        if (pro[i].ope == Ope::READ) {
          trans.tread(pro[i].key);
        } 
        else if (pro[i].ope == Ope::WRITE) {
          if (RMW) {
            trans.tread(pro[i].key);
            trans.twrite(pro[i].key, pro[i].val);
          }
          else
            trans.twrite(pro[i].key, pro[i].val);
        }
        else
          ERR;

        if (trans.status == TransactionStatus::aborted) {
          trans.abort();
          ++rsobject.localAbortCounts;
          goto RETRY;
        }
      }

      //commit - write phase
      trans.commit();
      ++rsobject.localCommitCounts;
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

  if (pthread_create(&t, nullptr, worker, (void *)myid)) ERR;

  return t;
}

int
main(const int argc, const char *argv[])
{
  Result rsobject;
  chkArg(argc, argv);
  makeDB();

  //displayDB();
  //displayPRO();

  pthread_t thread[THREAD_NUM];

  for (unsigned int i = 0; i < THREAD_NUM; i++) {
    thread[i] = threadCreate(i);
  }

  for (unsigned int i = 0; i < THREAD_NUM; i++) {
    pthread_join(thread[i], nullptr);
  }

  rsobject.displayTPS();
  //rsobject.displayAbortRate();
  //rsobject.displayAbortCounts();

  return 0;
}
