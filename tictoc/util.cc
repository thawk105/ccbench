
#include <sys/syscall.h> // syscall(SYS_gettid),
#include <sys/time.h>
#include <sys/types.h> // syscall(SYS_gettid),
#include <unistd.h> // syscall(SYS_gettid),

#include <atomic>
#include <bitset>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>

#include "../include/check.hpp"
#include "../include/inline.hpp"
#include "../include/debug.hpp"
#include "../include/random.hpp"
#include "../include/zipf.hpp"

#include "include/common.hpp"
#include "include/procedure.hpp"
#include "include/transaction.hpp"
#include "include/tuple.hpp"

using namespace std;

void 
chkArg(const int argc, char *argv[])
{
  if (argc != 10) {
    cout << "usage:./main TUPLE_NUM MAX_OPE THREAD_NUM RRATIO RMW ZIPF_SKEW YCSB CLOCK_PER_US EXTIME" << endl << endl;

    cout << "example:./main 200 10 24 50 on 0 on 2400 3" << endl << endl;

    cout << "TUPLE_NUM(int): total numbers of sets of key-value (1, 100), (2, 100)" << endl;
    cout << "MAX_OPE(int):    total numbers of operations" << endl;
    cout << "THREAD_NUM(int): total numbers of thread." << endl;
    cout << "RRATIO : read ratio [%%]" << endl;
    cout << "RMW : read modify write. on or off." << endl;
    cout << "ZIPF_SKEW : zipf skew. 0 ~ 0.999..." << endl;
    cout << "YCSB : on or off. switch makeProcedure function." << endl;
    cout << "CLOCK_PER_US: CPU_MHZ" << endl;
    cout << "EXTIME: execution time." << endl << endl;

    cout << "Tuple " << sizeof(Tuple) << endl;
    cout << "uint64_t_64byte " << sizeof(uint64_t_64byte) << endl;
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

  TUPLE_NUM = atoi(argv[1]);
  MAX_OPE = atoi(argv[2]);
  THREAD_NUM = atoi(argv[3]);
  RRATIO = atoi(argv[4]);
  string argrmw = argv[5];
  ZIPF_SKEW = atof(argv[6]);
  string argycsb = argv[7];
  CLOCK_PER_US = atof(argv[8]);
  EXTIME = atoi(argv[9]);

  if (RRATIO > 100) {
    cout << "rratio must be 0 ~ 10" << endl;
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
  else
    ERR;

  return;
}

bool
chkSpan(struct timeval &start, struct timeval &stop, long threshold)
{
  long diff = 0;
  diff += (stop.tv_sec - start.tv_sec) * 1000 * 1000 + (stop.tv_usec - start.tv_usec);
  if (diff > threshold) return true;
  else return false;
}

bool
chkClkSpan(uint64_t &start, uint64_t &stop, uint64_t threshold)
{
  uint64_t diff = 0;
  diff = stop - start;
  if (diff > threshold) return true;
  else return false;
}

void 
displayDB() 
{

  Tuple *tuple;

  for (unsigned int i = 0; i < TUPLE_NUM; ++i) {
    tuple = &Table[i];
    cout << "------------------------------" << endl; //-は30個
    cout << "key: " << i << endl;
    cout << "val: " << tuple->val << endl;
    cout << "TS_word: " << tuple->tsw.obj << endl;
    cout << "bit: " << static_cast<bitset<64>>(tuple->tsw.obj) << endl;
    cout << endl;
  }
}

void 
displayPRO(Procedure *pro) 
{
  for (unsigned int i = 0; i < MAX_OPE; i++) {
      cout << "(ope, key, val) = (";
    switch(pro[i].ope){
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

void
makeDB() 
{
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
    tmp->tsw.obj = 0;
    tmp->pre_tsw.obj = 0;
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
makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd, FastZipf &zipf) {
  for (unsigned int i = 0; i < MAX_OPE; ++i) {
    if ((rnd.next() % 100) < RRATIO)
      pro[i].ope = Ope::READ;
    else
      pro[i].ope = Ope::WRITE;

    pro[i].key = zipf() % TUPLE_NUM;
  }
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
