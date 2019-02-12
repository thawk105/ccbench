#include <atomic>
#include <bitset>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdint.h>
#include <stdlib.h>
#include <sys/syscall.h> // syscall(SYS_gettid),
#include <sys/time.h>
#include <sys/types.h> // syscall(SYS_gettid),
#include <unistd.h> // syscall(SYS_gettid),

#include "include/common.hpp"
#include "include/procedure.hpp"
#include "include/timeStamp.hpp"
#include "include/transaction.hpp"
#include "include/tuple.hpp"

#include "../include/debug.hpp"
#include "../include/random.hpp"
#include "../include/zipf.hpp"

using std::cout, std::endl;

void 
displayDB() 
{
  Tuple *tuple;
  Version *version;

  for (unsigned int i = 0; i < TUPLE_NUM; i++) {
    tuple = &Table[i % TUPLE_NUM];
    cout << "------------------------------" << endl; //-は30個
    cout << "key: " << tuple->key << endl;

    version = tuple->latest;
    while (version != NULL) {
      cout << "val: " << version->val << endl;
      
      switch (version->status) {
        case VersionStatus::invalid:
          cout << "status:  invalid";
          break;
        case VersionStatus::pending:
          cout << "status:  pending";
          break;
        case VersionStatus::aborted:
          cout << "status:  aborted";
          break;
        case VersionStatus::committed:
          cout << "status:  committed";
          break;
        case VersionStatus::deleted:
          cout << "status:  deleted";
          break;
        default:
          cout << "status error";
          break;
      }
      cout << endl;

      cout << "wts: " << version->wts << endl;
      cout << "bit: " << static_cast<bitset<64> >(version->wts) << endl;
      cout << "rts: " << version->rts << endl;
      cout << "bit: " << static_cast<bitset<64> >(version->rts) << endl;
      cout << endl;

      version = version->next;
    }
    cout << endl;
  }
}

void 
displayPRO(Procedure *pro) 
{
  for (unsigned int i = 0; i < MAX_OPE; ++i) {
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
displayMinRts() 
{
  cout << "MinRts:  " << MinRts << endl << endl;
}

void 
displayMinWts() 
{
  cout << "MinWts:  " << MinWts << endl << endl;
}

void 
displayThreadWtsArray() 
{
  cout << "ThreadWtsArray:" << endl;
  for (unsigned int i = 0; i < THREAD_NUM; i++) {
    cout << "thid " << i << ": " << ThreadWtsArray[i].obj << endl;
  }
  cout << endl << endl;
}

void 
displayThreadRtsArray() 
{
  cout << "ThreadRtsArray:" << endl;
  for (unsigned int i = 0; i < THREAD_NUM; i++) {
    cout << "thid " << i << ": " << ThreadRtsArray[i].obj << endl;
    }
    cout << endl << endl;
}

void 
displaySLogSet() 
{
  if (!GROUP_COMMIT) {
  }
  else {
    if (S_WAL) {
      SwalLock.w_lock();
      for (unsigned int i = 0; i < GROUP_COMMIT_INDEX[0].obj; ++i) {
        printf("SLogSet[%d]->key, val = (%d, %d)\n", i, SLogSet[i]->key, SLogSet[i]->val);
      }
      SwalLock.w_unlock();

      //if (i == 0) printf("SLogSet is empty\n");
    }
  }
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
makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd) {
  bool ronly = true;

  for (unsigned int i = 0; i < MAX_OPE; ++i) {
    if ((rnd.next() % 100) < RRATIO) {
      pro[i].ope = Ope::READ;
    } else {
      ronly = false;
      pro[i].ope = Ope::WRITE;
    }
    pro[i].key = rnd.next() % TUPLE_NUM;
    pro[i].val = rnd.next() % TUPLE_NUM;
  }
  
  if (ronly) pro[0].ronly = true;
  else pro[0].ronly = false;
}

void 
makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd, FastZipf &zipf) {
  bool ronly = true;

  for (unsigned int i = 0; i < MAX_OPE; ++i) {
    if ((rnd.next() % 100) < RRATIO) {
      pro[i].ope = Ope::READ;
    }
    else {
      ronly = false;
      pro[i].ope = Ope::WRITE;
    }

    pro[i].key = zipf() % TUPLE_NUM;
    pro[i].val = rnd.next() % TUPLE_NUM;
  }
  
  if (ronly) pro[0].ronly = true;
  else pro[0].ronly = false;
}

void
makeDB(uint64_t *initial_wts)
{
  Tuple *tmp;
  Version *verTmp;
  Xoroshiro128Plus rnd;
  rnd.init();

  try {
    if (posix_memalign((void**)&Table, 64, TUPLE_NUM * sizeof(Tuple)) != 0) ERR;
    for (unsigned int i = 0; i < TUPLE_NUM; i++) {
      Table[i].latest = new Version();
    }
  } catch (bad_alloc) {
    ERR;
  }

  TimeStamp tstmp;
  tstmp.generateTimeStampFirst(0);
  *initial_wts = tstmp.ts;
  for (unsigned int i = 0; i < TUPLE_NUM; i++) {
    tmp = &Table[i];
    tmp->min_wts = tstmp.ts;
    tmp->key = i;
    tmp->gClock.store(0, std::memory_order_release);
    verTmp = tmp->latest.load();
    verTmp->wts.store(tstmp.ts, std::memory_order_release);;
    verTmp->status.store(VersionStatus::committed, std::memory_order_release);
    verTmp->key = i;
    verTmp->val = rnd.next() % (TUPLE_NUM * 10);
  }
}

void
setThreadAffinity(int myid)
{
  pid_t pid = syscall(SYS_gettid);
  cpu_set_t cpu_set;

  CPU_ZERO(&cpu_set);
  CPU_SET(myid % sysconf(_SC_NPROCESSORS_CONF), &cpu_set);

  if (sched_setaffinity(pid, sizeof(cpu_set_t), &cpu_set) != 0)
    ERR;
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
