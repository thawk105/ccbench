#include <atomic>
#include <bitset>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h> // syscall(SYS_gettid),
#include <sys/time.h>
#include <sys/types.h> // syscall(SYS_gettid),
#include <unistd.h> // syscall(SYS_gettid),

#include "../include/cache_line_size.hpp"
#include "../include/check.hpp"
#include "../include/debug.hpp"
#include "../include/random.hpp"
#include "../include/zipf.hpp"

#include "include/common.hpp"
#include "include/procedure.hpp"
#include "include/timeStamp.hpp"
#include "include/transaction.hpp"
#include "include/tuple.hpp"

using std::cout, std::endl;

void 
chkArg(const int argc, char *argv[])
{
  if (argc != 16) {
    cout << "usage: ./cicada.exe TUPLE_NUM MAX_OPE THREAD_NUM RRATIO RMW ZIPF_SKEW YCSB WAL GROUP_COMMIT CPU_MHZ IO_TIME_NS GROUP_COMMIT_TIMEOUT_US LOCK_RELEASE_METHOD GC_INTER_US EXTIME" << endl << endl;
    cout << "example:./main 200 10 24 50 off 0 on off off 2400 5 2 E 10 3" << endl << endl;
    cout << "TUPLE_NUM(int): total numbers of sets of key-value (1, 100), (2, 100)" << endl;
    cout << "MAX_OPE(int):    total numbers of operations" << endl;
    cout << "THREAD_NUM(int): total numbers of worker thread." << endl;
    cout << "RRATIO: read ratio [%%]" << endl;
    cout << "RMW : read modify write. on or off." << endl;
    cout << "ZIPF_SKEW : zipf skew. 0 ~ 0.999..." << endl;
    cout << "YCSB : on or off. switch makeProcedure function." << endl;
    cout << "WAL: P or S or off." << endl;
    cout << "GROUP_COMMIT:  unsigned integer or off, i reccomend off or 3" << endl;
    cout << "CPU_MHZ(float):  your cpuMHz. used by calculate time of yours 1clock." << endl;
    cout << "IO_TIME_NS: instead of exporting to disk, delay is inserted. the time(nano seconds)." << endl;
    cout << "GROUP_COMMIT_TIMEOUT_US: Invocation condition of group commit by timeout(micro seconds)." << endl;
    cout << "LOCK_RELEASE_METHOD: E or NE or N. Early lock release(tanabe original) or Normal Early Lock release or Normal lock release." << endl;
    cout << "GC_INTER_US: garbage collection interval [usec]" << endl;
    cout << "EXTIME: execution time [sec]" << endl << endl;

    cout << "Tuple " << sizeof(Tuple) << endl;
    cout << "Version " << sizeof(Version) << endl;
    cout << "TimeStamp " << sizeof(TimeStamp) << endl;
    cout << "Procedure " << sizeof(Procedure) << endl;

    exit(0);
  }

  chkInt(argv[1]);
  chkInt(argv[2]);
  chkInt(argv[3]);
  chkInt(argv[4]);
  chkInt(argv[10]);
  chkInt(argv[11]);
  chkInt(argv[12]);
  chkInt(argv[14]);
  chkInt(argv[15]);

  TUPLE_NUM = atoi(argv[1]);
  MAX_OPE = atoi(argv[2]);
  THREAD_NUM = atoi(argv[3]);
  RRATIO = atoi(argv[4]);
  string argrmw = argv[5];
  ZIPF_SKEW = atof(argv[6]);
  string argycsb = argv[7];
  string argwal = argv[8];
  string arggrpc = argv[9];
  CLOCK_PER_US = atof(argv[10]);
  IO_TIME_NS = atof(argv[11]);
  GROUP_COMMIT_TIMEOUT_US = atoi(argv[12]);
  string arglr = argv[13];
  GC_INTER_US = atoi(argv[14]);
  EXTIME = atoi(argv[15]);

  if (RRATIO > 100) {
    cout << "rratio [%%] must be 0 ~ 100)" << endl;
    ERR;
  }

  if (ZIPF_SKEW >= 1) {
    cout << "ZIPF_SKEW must be 0 ~ 0.999..." << endl;
    ERR;
  }

  if (argrmw == "on")
    RMW = true;
  else if (argrmw == "off")
    RMW = false;
  else
    ERR;

  if (argycsb == "on") {
    YCSB = true;
  }
  else if (argycsb == "off") {
    YCSB = false;
  }
  else ERR;

  if (argwal == "P")  {
    P_WAL = true;
    S_WAL = false;
  } 
  else if (argwal == "S") {
    P_WAL = false;
    S_WAL = true;
  } 
  else if (argwal == "off") {
    P_WAL = false;
    S_WAL = false;

    if (arggrpc != "off") {
      printf("i don't implement below.\n\
P_WAL off, S_WAL off, GROUP_COMMIT number.\n\
usage: P_WAL or S_WAL is selected. \n\
P_WAL and S_WAL isn't selected, GROUP_COMMIT must be off. this isn't logging. performance is concurrency control only.\n\n");
      exit(0);
    }
  }
  else {
    printf("WAL must be P or S or off\n");
    exit(0);
  }

  if (arggrpc == "off") GROUP_COMMIT = 0;
  else if (chkInt(argv[9])) {
      GROUP_COMMIT = atoi(argv[9]);
  }
  else {
    printf("GROUP_COMMIT(argv[9]) must be unsigned integer or off\n");
    exit(0);
  }

  if (CLOCK_PER_US < 100) {
    printf("CPU_MHZ is less than 100. are you really?\n");
    exit(0);
  }

  if (arglr == "N") {
    NLR = true;
    ELR = false;
  }
  else if (arglr == "E") {
    NLR = false;
    ELR = true;
  }
  else {
    printf("LockRelease(argv[13]) must be E or N\n");
    exit(0);
  }

  try {
    if (posix_memalign((void**)&ThreadRtsArrayForGroup, CACHE_LINE_SIZE, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
    if (posix_memalign((void**)&ThreadWtsArray, CACHE_LINE_SIZE, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
    if (posix_memalign((void**)&ThreadRtsArray, CACHE_LINE_SIZE, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
    if (posix_memalign((void**)&GROUP_COMMIT_INDEX, CACHE_LINE_SIZE, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
    if (posix_memalign((void**)&GROUP_COMMIT_COUNTER, CACHE_LINE_SIZE, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
    if (posix_memalign((void**)&GCFlag, CACHE_LINE_SIZE, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
    if (posix_memalign((void**)&GCExecuteFlag, CACHE_LINE_SIZE, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
    
    SLogSet = new Version*[(MAX_OPE) * (GROUP_COMMIT)]; 
    PLogSet = new Version**[THREAD_NUM];

    for (unsigned int i = 0; i < THREAD_NUM; ++i) {
      PLogSet[i] = new Version*[(MAX_OPE) * (GROUP_COMMIT)];
    }
  } catch (bad_alloc) {
    ERR;
  }
  //init
  for (unsigned int i = 0; i < THREAD_NUM; ++i) {
    GCFlag[i].obj = 0;
    GCExecuteFlag[i].obj = 0;
    GROUP_COMMIT_INDEX[i].obj = 0;
    GROUP_COMMIT_COUNTER[i].obj = 0;
    ThreadRtsArray[i].obj = 0;
    ThreadWtsArray[i].obj = 0;
    ThreadRtsArrayForGroup[i].obj = 0;
  }
}

void 
displayDB() 
{
  Tuple *tuple;
  Version *version;

  for (unsigned int i = 0; i < TUPLE_NUM; i++) {
    tuple = &Table[i % TUPLE_NUM];
    cout << "------------------------------" << endl; //-は30個
    cout << "key: " << i << endl;

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
        //printf("SLogSet[%d]->key, val = (%d, %d)\n", i, SLogSet[i]->key, SLogSet[i]->val);
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
    if (posix_memalign((void**)&Table, CACHE_LINE_SIZE, TUPLE_NUM * sizeof(Tuple)) != 0) ERR;
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
    tmp->gClock.store(0, std::memory_order_release);
    verTmp = tmp->latest.load();
    verTmp->wts.store(tstmp.ts, std::memory_order_release);;
    verTmp->status.store(VersionStatus::committed, std::memory_order_release);
    verTmp->val[0] = 'a';
    verTmp->val[1] = '\0';
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
