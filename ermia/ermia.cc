
#include <ctype.h>  //isdigit, 
#include <pthread.h>
#include <string.h> //strlen,
#include <sys/syscall.h>  //syscall(SYS_gettid),  
#include <sys/types.h>  //syscall(SYS_gettid),
#include <time.h>
#include <unistd.h> //syscall(SYS_gettid), 

#include <iostream>
#include <string> //string

#define GLOBAL_VALUE_DEFINE

#include "../include/debug.hpp"
#include "../include/int64byte.hpp"
#include "../include/random.hpp"
#include "../include/tsc.hpp"
#include "../include/zipf.hpp"

#include "include/common.hpp"
#include "include/garbageCollection.hpp"
#include "include/result.hpp"
#include "include/transaction.hpp"

using namespace std;

extern bool chkClkSpan(uint64_t &start, uint64_t &stop, uint64_t threshold);
extern void makeDB();
extern void makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd);
extern void makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd, FastZipf &zipf);
extern void naiveGarbageCollection();
extern inline uint64_t rdtsc();
extern void setThreadAffinity(int myid);
extern void waitForReadyOfAllThread();

static bool
chkInt(const char *arg)
{
  for (unsigned int i = 0; i < strlen(arg); ++i) {
    if (!isdigit(arg[i])) {
      cout << std::string(arg) << " is not a number." << endl;
      exit(0);
    }
  }
  return true;
}

static void
chkArg(const int argc, const char *argv[])
{
  if (argc != 10) {
  //if (argc != 1) {
    cout << "usage: ./ermia.exe TUPLE_NUM MAX_OPE THREAD_NUM RRATIO ZIPF_SKEW YCSB CPU_MHZ GC_INTER_US EXTIME" << endl;
    cout << "example: ./ermia.exe 200 10 24 50 0 OFF 2400 10 3" << endl;
    cout << "TUPLE_NUM(int): total numbers of sets of key-value" << endl;
    cout << "MAX_OPE(int): total numbers of operations" << endl;
    cout << "THREAD_NUM(int): total numbers of worker thread" << endl;
    cout << "RRATIO : read ratio [%%]" << endl;
    cout << "ZIPF_SKEW : zipf skew. 0 ~ 0.999..." << endl;
    cout << "YCSB : ON or OFF. switch makeProcedure function." << endl;
    cout << "CPU_MHZ(float): your cpuMHz. used by calculate time of yorus 1clock" << endl;
    cout << "GC_INTER_US : garbage collection interval [usec]" << endl;
    cout << "EXTIME: execution time [sec]" << endl;

    cout << "Tuple " << sizeof(Tuple) << endl;
    cout << "Version " << sizeof(Version) << endl;
    cout << "uint64_t_64byte " << sizeof(uint64_t_64byte) << endl;
    cout << "TransactionTable " << sizeof(TransactionTable) << endl;

    exit(0);
  }

  //test
  //TUPLE_NUM = 10;
  //MAX_OPE = 10;
  //THREAD_NUM = 2;
  //PRO_NUM = 100;
  //READ_RATIO = 0.5;
  //CLOCK_PER_US = 2400;
  //EXTIME = 3;
  //-----
  
  
  chkInt(argv[1]);
  chkInt(argv[2]);
  chkInt(argv[3]);
  chkInt(argv[4]);
  chkInt(argv[7]);
  chkInt(argv[8]);

  TUPLE_NUM = atoi(argv[1]);
  MAX_OPE = atoi(argv[2]);
  THREAD_NUM = atoi(argv[3]);
  RRATIO = atoi(argv[4]);
  ZIPF_SKEW = atof(argv[5]);
  string argst = argv[6];
  CLOCK_PER_US = atof(argv[7]);
  GC_INTER_US = atoi(argv[8]);
  EXTIME = atoi(argv[9]);

  if (THREAD_NUM < 2) {
    cout << "1 thread is leader thread. \nthread number 1 is no worker thread, so exit." << endl;
    ERR;
  }
  if (RRATIO > 100) {
    cout << "rratio [%%] must be 0 ~ 100" << endl;
    ERR;
  }

  if (ZIPF_SKEW >= 1) {
    cout << "ZIPF_SKEW must be 0 ~ 0.999..." << endl;
    ERR;
  }

  if (argst == "ON")
    YCSB = true;
  else if (argst == "OFF")
    YCSB = false;
  else 
    ERR;

  if (CLOCK_PER_US < 100) {
    cout << "CPU_MHZ is less than 100. are your really?" << endl;
    ERR;
  }

  try {
    TMT = new TransactionTable*[THREAD_NUM];
  } catch (bad_alloc) {
    ERR;
  }

  for (unsigned int i = 0; i < THREAD_NUM; ++i) {
    TMT[i] = new TransactionTable(0, 0, UINT32_MAX, 0, TransactionStatus::inFlight);
  }


}

static void *
manager_worker(void *arg)
{
  // start, inital work
  int *myid = (int *)arg;
  GarbageCollection gcobject;
  Result rsobject;
  
  setThreadAffinity(*myid);
  gcobject.decideFirstRange();
  waitForReadyOfAllThread();
  // end, initial work
  
  
  rsobject.Bgn = rdtsc();
  for (;;) {
    usleep(1);
    rsobject.End = rdtsc();
    if (chkClkSpan(rsobject.Bgn, rsobject.End, EXTIME * 1000 * 1000 * CLOCK_PER_US)) {
      Finish.store(true, std::memory_order_release);
      return nullptr;
    }

    if (gcobject.chkSecondRange()) {
      gcobject.decideGcThreshold();
      gcobject.mvSecondRangeToFirstRange();
    }
  }

  return nullptr;
}


static void *
worker(void *arg)
{
  int *myid = (int *)arg;
  Transaction trans(*myid, MAX_OPE);
  Procedure pro[MAX_OPE];
  Xoroshiro128Plus rnd;
  rnd.init();
  FastZipf zipf(&rnd, ZIPF_SKEW, TUPLE_NUM);
  
  setThreadAffinity(*myid);
  //printf("Thread #%d: on CPU %d\n", *myid, sched_getcpu());
  //printf("sysconf(_SC_NPROCESSORS_CONF) %ld\n", sysconf(_SC_NPROCESSORS_CONF));
  waitForReadyOfAllThread();
  
  trans.gcstart = rdtsc();
  //start work (transaction)
  try {
    //printf("Thread #%d: on CPU %d\n", *myid, sched_getcpu());
    for(;;) {
      if (YCSB) makeProcedure(pro, rnd, zipf);
      else makeProcedure(pro, rnd);

      asm volatile ("" ::: "memory");
RETRY:

      if (Finish.load(std::memory_order_acquire)) {
        trans.rsobject.sumUpAbortCounts();
        trans.rsobject.sumUpCommitCounts();
        trans.rsobject.sumUpGCVersionCounts();
        trans.rsobject.sumUpGCTMTElementsCounts();
        trans.rsobject.sumUpGCCounts();
        //printf("Th #%d : CommitCounts : %lu : AbortCounts : %lu\n", trans.thid, rsobject.localCommitCounts, rsobject.localAbortCounts);
        return nullptr;
      }

      //-----
      //transaction begin
      trans.tbegin();
      for (unsigned int i = 0; i < MAX_OPE; ++i) {
        if (pro[i].ope == Ope::READ) {
          trans.ssn_tread(pro[i].key);
          //if (trans.status == TransactionStatus::aborted) NNN;
        } else {
          trans.ssn_twrite(pro[i].key, pro[i].val);
          //if (trans.status == TransactionStatus::aborted) NNN;
        }

        if (trans.status == TransactionStatus::aborted) {
          trans.abort();
          goto RETRY;
        }
      }

      //trans.ssn_commit();
      trans.ssn_parallel_commit();

      if (trans.status == TransactionStatus::aborted) {
        trans.abort();
        goto RETRY;
      }

      // maintenance phase
      // garbage collection
      trans.mainte();
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

  if (*myid == 0) {
    if (pthread_create(&t, nullptr, manager_worker, (void *)myid)) ERR;
  } else {
    if (pthread_create(&t, nullptr, worker, (void *)myid)) ERR;
  }

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

  for (unsigned int i = 0; i < THREAD_NUM; ++i) {
    thread[i] = threadCreate(i);
  }

  for (unsigned int i = 0; i < THREAD_NUM; ++i) {
    pthread_join(thread[i], nullptr);
  }

  //displayDB();

  rsobject.displayTPS();
  //rsobject.displayGCCounts();
  //rsobject.displayAbortRate();
  //rsobject.displayGCVersionCountsPS();
  //rsobject.displayGCTMTElementsCountsPS();

  return 0;
}
