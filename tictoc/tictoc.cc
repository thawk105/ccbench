
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

#include "../include/debug.hpp"
#include "../include/random.hpp"
#include "../include/tsc.hpp"
#include "../include/zipf.hpp"

#include "include/common.hpp"
#include "include/result.hpp"
#include "include/transaction.hpp"

using namespace std;

extern void chkArg(const int argc, char *argv[]);
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
  //printf("sysconf(_SC_NPROCESSORS_CONF) %d\n", sysconf(_SC_NPROCESSORS_CONF));
  waitForReadyOfAllThread();
  
  if (*myid == 0) rsobject.Bgn = rdtsc();

  try {
    for (;;) {
      if (YCSB)
        makeProcedure(pro, rnd, zipf);
      else
        makeProcedure(pro, rnd);

RETRY:

      trans.tbegin();

      // finish judge
      if (*myid == 0) {
        rsobject.End = rdtsc();
        if (chkClkSpan(rsobject.Bgn, rsobject.End, EXTIME*1000*1000 * CLOCK_PER_US)) {
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
      // -----

      for (unsigned int i = 0; i < MAX_OPE; ++i) {
        if (pro[i].ope == Ope::READ) {
          trans.tread(pro[i].key);
          if (trans.status == TransactionStatus::aborted) {
            trans.abort();
            ++rsobject.localAbortCounts;
            goto RETRY;
          }
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
      }
      
      //Validation phase
      if (trans.validationPhase()) {
        trans.writePhase();
        ++rsobject.localCommitCounts;
      } else {
        trans.abort();
        ++rsobject.localAbortCounts;
        goto RETRY;
      }
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
main(int argc, char *argv[]) 
{
  Result rsobject;
  chkArg(argc, argv);
  makeDB();
  
  //displayDB();
  //displayPRO();

  pthread_t thread[THREAD_NUM];

  for (unsigned int i = 0; i < THREAD_NUM; ++i) {
    thread[i] = threadCreate(i);
  }

  for (unsigned int i = 0; i < THREAD_NUM; ++i) {
    pthread_join(thread[i], nullptr);
  }

  //displayDB();

  rsobject.displayTPS();
  //rsobject.displayAbortRate();

  return 0;
}
