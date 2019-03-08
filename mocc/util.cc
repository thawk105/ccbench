
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h> // syscall(SYS_gettid),
#include <sys/types.h> // syscall(SYS_gettid),
#include <unistd.h> // syscall(SYS_gettid),

#include <algorithm>
#include <atomic>
#include <bitset>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <limits>
#include <random>

#include "../include/check.hpp"
#include "../include/debug.hpp"
#include "../include/random.hpp"
#include "../include/zipf.hpp"

#include "include/common.hpp"
#include "include/procedure.hpp"
#include "include/tuple.hpp"

using namespace std;

void
chkArg(const int argc, char *argv[])
{
  if (argc != 11) {
    cout << "usage: ./mocc.exe TUPLE_NUM MAX_OPE THREAD_NUM RRATIO RMW ZIPF_SKEW YCSB CPU_MHZ EPOCH_TIME EXTIME" << endl;
    cout << "example: ./mocc.exe 200 10 24 50 off 0 off 2400 40 3" << endl;

    cout << "TUPLE_NUM(int): total numbers of sets of key-value" << endl;
    cout << "MAX_OPE(int): total numbers of operations" << endl;
    cout << "THREAD_NUM(int): total numbers of worker thread" << endl;
    cout << "RRATIO : read ratio [%%]" << endl;
    cout << "RMW : read modify write. on or off." << endl;
    cout << "ZIPF_SKEW : zipf skew. 0 ~ 0.999..." << endl;
    cout << "YCSB : on or off. switch makeProcedure function." << endl;
    cout << "CPU_MHZ(float): your cpuMHz. used by calculate time of yorus 1clock" << endl;
    cout << "EPOCH_TIME(unsigned int)(ms): Ex. 40" << endl;
    cout << "EXTIME: execution time [sec]" << endl;

    cout << "Tuple " << sizeof(Tuple) << endl;
    cout << "RWLock " << sizeof(RWLock) << endl;
    cout << "MQLock " << sizeof(MQLock) << endl;
    cout << "uint64_t_64byte " << sizeof(uint64_t_64byte) << endl;
    cout << "Xoroshiro128Plus " << sizeof(Xoroshiro128Plus) << endl;
    cout << "pthread_mutex_t" << sizeof(pthread_mutex_t) << endl;
    cout << "KEY_SIZE : " << KEY_SIZE << endl;
    cout << "VAL_SIZE : " << VAL_SIZE << endl;

    exit(0);
  }


  chkInt(argv[1]);
  chkInt(argv[2]);
  chkInt(argv[3]);
  chkInt(argv[4]);
  chkInt(argv[8]);
  chkInt(argv[9]);
  chkInt(argv[10]);

  TUPLE_NUM = atoi(argv[1]);
  MAX_OPE = atoi(argv[2]);
  THREAD_NUM = atoi(argv[3]);
  RRATIO = atoi(argv[4]);
  string argrmw = argv[5];
  ZIPF_SKEW = atof(argv[6]);
  string argycsb = argv[7];
  CLOCK_PER_US = atof(argv[8]);
  EPOCH_TIME = atoi(argv[9]);
  EXTIME = atoi(argv[10]);

  if (RRATIO > 100) {
    cout << "rratio (* 10 \%) must be 0 ~ 10)" << endl;
    ERR;
  }

  if (argrmw == "on")
    RMW = true;
  else if (argrmw == "off")
    RMW = false;
  else
    ERR;

  if (ZIPF_SKEW >= 1) {
    cout << "ZIPF_SKEW must be 0 ~ 0.999..." << endl;
    ERR;
  }

  if (argycsb == "on")
    YCSB = true;
  else if (argycsb == "off")
    YCSB = false;
  else ERR;

  if (CLOCK_PER_US < 100) {
    cout << "CPU_MHZ is less than 100. are your really?" << endl;
    ERR;
  }

  try {
    if (posix_memalign((void**)&Start, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
    if (posix_memalign((void**)&Stop, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
    if (posix_memalign((void**)&ThLocalEpoch, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
#ifdef MQLOCK
    //if (posix_memalign((void**)&MQLNodeList, 64, (THREAD_NUM + 3) * sizeof(MQLNode)) != 0) ERR;
    MQLNodeTable = new MQLNode*[THREAD_NUM + 3];
    for (unsigned int i = 0; i < THREAD_NUM + 3; ++i)
      MQLNodeTable[i] = new MQLNode[TUPLE_NUM];
#endif // MQLOCK
  } catch (bad_alloc) {
    ERR;
  }

  for (unsigned int i = 0; i < THREAD_NUM; ++i) {
    ThLocalEpoch[i].obj = 0;
  }
}

void 
displayDB()
{
  Tuple *tuple;

  for (unsigned int i = 0; i < TUPLE_NUM; ++i) {
    tuple = &Table[i];
    cout << "----------" << endl; // - is 10
    cout << "key: " << i << endl;
    cout << "val: " << tuple->val << endl;
    cout << "lockctr: " << tuple->rwlock.counter << endl;
    cout << "TIDword: " << tuple->tidword.obj << endl;
    cout << "bit: " << static_cast<bitset<64>>(tuple->tidword.obj) << endl;
    cout << endl;
  }
}

void
displayLockedTuple()
{
  for (unsigned int i = 0; i < TUPLE_NUM; ++i) {
#ifdef RWLOCK
    if (Table[i].rwlock.counter.load(memory_order_relaxed) == -1) {
#endif // RWLOCK
      cout << "key : " << i << " is locked!." << endl;
    }
  }
}

void 
displayPRO(Procedure *pro)
{
  for (unsigned int i = 0; i < MAX_OPE; ++i) {
    cout << "(ope, key, val) = (";
    switch (pro[i].ope) {
      case Ope::READ:
      cout << "READ";
        break;
      case Ope::WRITE:
        cout << "WRITE";
        break;
      default:
      break;
    }
    cout << ", " << pro[i].key
      << ", " << pro[i].val << ")" << endl;
  }
}

bool
chkClkSpan(uint64_t &start, uint64_t &stop, uint64_t threshold)
{
  uint64_t diff = 0;
  diff = stop - start;
  if (diff > threshold) return true;
  else return false;
}

void makeDB() {
  Tuple *tmp;
  Xoroshiro128Plus rnd;
  rnd.init();

  try {
    if (posix_memalign((void**)&Table, 64, (TUPLE_NUM) * sizeof(Tuple)) != 0) ERR;
  } catch (bad_alloc) {
    ERR;
  }

  for (unsigned int i = 0; i < TUPLE_NUM; ++i) {
    tmp = &Table[i];

    tmp->tidword.epoch = 1;
    tmp->tidword.tid = 0;

    tmp->epotemp.epoch = 1;
    tmp->epotemp.temp = 0;

    tmp->val[0] = 'a'; tmp->val[1] = '\0';
  }

}

void 
makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd) 
{
  for (unsigned int i = 0; i < MAX_OPE; ++i) {
    if ((rnd.next() % 100) < RRATIO)
      pro[i].ope = Ope::READ;
    else
      pro[i].ope = Ope::WRITE;
    
    pro[i].key = rnd.next() % TUPLE_NUM;
  }
}

void 
makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd, FastZipf &zipf)
{
  for (unsigned int i = 0; i < MAX_OPE; ++i) {
    if ((rnd.next() % 100) < RRATIO)
      pro[i].ope = Ope::READ;
    else
      pro[i].ope = Ope::WRITE;

    pro[i].key = zipf() % TUPLE_NUM;
  }
}

void
setThreadAffinity(int myid)
{
#ifdef Linux
  pid_t pid = syscall(SYS_gettid);
  cpu_set_t cpu_set;

  CPU_ZERO(&cpu_set);
  CPU_SET(myid % sysconf(_SC_NPROCESSORS_CONF), &cpu_set);

  if (sched_setaffinity(pid, sizeof(cpu_set_t), &cpu_set) != 0)
    ERR;
#endif // Linux

  return;
}

void
waitForReadyOfAllThread()
{
  unsigned int expected, desired;
  expected = Running.load(std::memory_order_acquire);
  do {
    desired = expected + 1;
  } while (!Running.compare_exchange_weak(expected, desired, std::memory_order_acq_rel, std::memory_order_acquire));

  while (Running.load(std::memory_order_acquire) != THREAD_NUM);
  return;
}

