
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>  // syscall(SYS_gettid),
#include <sys/types.h>    // syscall(SYS_gettid),
#include <unistd.h>       // syscall(SYS_gettid),
#include <algorithm>
#include <atomic>
#include <bitset>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>

#include "../include/atomic_wrapper.hh"
#include "../include/check.hh"
#include "../include/config.hh"
#include "../include/debug.hh"
#include "../include/masstree_wrapper.hh"
#include "../include/procedure.hh"
#include "../include/random.hh"
#include "../include/result.hh"
#include "../include/tsc.hh"
#include "../include/zipf.hh"
#include "include/atomic_tool.hh"
#include "include/common.hh"
#include "include/tuple.hh"

using namespace std;

extern size_t decideParallelBuildNumber(size_t tuplenum);

void chkArg(const int argc, char *argv[]) {
  if (argc != 12) {
    cout << "usage: ./mocc.exe TUPLE_NUM MAX_OPE THREAD_NUM RRATIO RMW "
            "ZIPF_SKEW YCSB CPU_MHZ EPOCH_TIME PER_XX_TEMP EXTIME"
         << endl;
    cout << "example: ./mocc.exe 200 10 224 50 off 0 on 2100 40 4096 3" << endl;

    cout << "TUPLE_NUM(int): total numbers of sets of key-value" << endl;
    cout << "MAX_OPE(int): total numbers of operations" << endl;
    cout << "THREAD_NUM(int): total numbers of worker thread" << endl;
    cout << "RRATIO : read ratio [%%]" << endl;
    cout << "RMW : read modify write. on or off." << endl;
    cout << "ZIPF_SKEW : zipf skew. 0 ~ 0.999..." << endl;
    cout << "YCSB : on or off. switch makeProcedure function." << endl;
    cout
        << "CPU_MHZ(float): your cpuMHz. used by calculate time of yorus 1clock"
        << endl;
    cout << "EPOCH_TIME(unsigned int)(ms): Ex. 40" << endl;
    cout << "PER_XX_TEMP:\tWhat record size (bytes) does it integrate about "
            "temperature statistics."
         << endl;
    cout << "EXTIME: execution time [sec]" << endl;

    cout << "Tuple " << sizeof(Tuple) << endl;
    cout << "Epotemp:\t" << sizeof(Epotemp) << endl;
    cout << "RWLock " << sizeof(RWLock) << endl;
    cout << "MQLock " << sizeof(MQLock) << endl;
    cout << "uint64_t_64byte " << sizeof(uint64_t_64byte) << endl;
    cout << "Xoroshiro128Plus " << sizeof(Xoroshiro128Plus) << endl;
    cout << "pthread_mutex_t" << sizeof(pthread_mutex_t) << endl;
    cout << "KEY_SIZE : " << KEY_SIZE << endl;
    cout << "VAL_SIZE : " << VAL_SIZE << endl;
    cout << "MASSTREE_USE : " << MASSTREE_USE << endl;
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
  CLOCKS_PER_US = atof(argv[8]);
  EPOCH_TIME = atoi(argv[9]);
  PER_XX_TEMP = atoi(argv[10]);
  EXTIME = atoi(argv[11]);

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
  else
    ERR;

  if (CLOCKS_PER_US < 100) {
    cout << "CPU_MHZ is less than 100. are your really?" << endl;
    ERR;
  }

  if (PER_XX_TEMP < sizeof(Tuple)) {
    cout << "PER_XX_TEMP's minimum is sizeof(Tuple) " << sizeof(Tuple) << endl;
    ERR;
  }

  if (posix_memalign((void **)&Start, 64,
                     THREAD_NUM * sizeof(uint64_t_64byte)) != 0)
    ERR;
  if (posix_memalign((void **)&Stop, 64,
                     THREAD_NUM * sizeof(uint64_t_64byte)) != 0)
    ERR;
  if (posix_memalign((void **)&ThLocalEpoch, 64,
                     THREAD_NUM * sizeof(uint64_t_64byte)) != 0)
    ERR;
#ifdef MQLOCK
  // if (posix_memalign((void**)&MQLNodeList, 64, (THREAD_NUM + 3) *
  // sizeof(MQLNode)) != 0) ERR;
  MQLNodeTable = new MQLNode *[THREAD_NUM + 3];
  for (unsigned int i = 0; i < THREAD_NUM + 3; ++i)
    MQLNodeTable[i] = new MQLNode[TUPLE_NUM];
#endif  // MQLOCK

  for (unsigned int i = 0; i < THREAD_NUM; ++i) {
    ThLocalEpoch[i].obj_ = 0;
  }
}

void displayDB() {
  Tuple *tuple;

  for (unsigned int i = 0; i < TUPLE_NUM; ++i) {
    tuple = &Table[i];
    cout << "----------" << endl;  // - is 10
    cout << "key: " << i << endl;
    cout << "val: " << tuple->val_ << endl;
    cout << "lockctr: " << tuple->rwlock_.counter_ << endl;
    cout << "TIDword: " << tuple->tidword_.obj_ << endl;
    cout << "bit: " << static_cast<bitset<64>>(tuple->tidword_.obj_) << endl;
    cout << endl;
  }
}

void displayLockedTuple() {
  for (unsigned int i = 0; i < TUPLE_NUM; ++i) {
#ifdef RWLOCK
    if (Table[i].rwlock_.counter_.load(memory_order_relaxed) == -1) {
#endif  // RWLOCK
      cout << "key : " << i << " is locked!." << endl;
    }
  }
}

void partTableInit([[maybe_unused]] size_t thid, uint64_t start, uint64_t end) {
#if MASSTREE_USE
  MasstreeWrapper<Tuple>::thread_init(thid);
#endif

  for (uint64_t i = start; i <= end; ++i) {
    Tuple *tmp;
    tmp = &Table[i];
    tmp->tidword_.epoch = 1;
    tmp->tidword_.tid = 0;
    tmp->val_[0] = 'a';
    tmp->val_[1] = '\0';

    Epotemp epotemp(0, 1);
    size_t epotemp_index = i * sizeof(Tuple) / PER_XX_TEMP;
    // cout << "key:\t" << i << ", ep_index:\t" << ep_index << endl;
    storeRelease(EpotempAry[epotemp_index].obj_, epotemp.obj_);
#if MASSTREE_USE
    MT.insert_value(i, tmp);
#endif
  }
}

void makeDB() {
  if (posix_memalign((void **)&Table, PAGE_SIZE, TUPLE_NUM * sizeof(Tuple)) !=
      0)
    ERR;
#if dbs11
  if (madvise((void *)Table, (TUPLE_NUM) * sizeof(Tuple), MADV_HUGEPAGE) != 0)
    ERR;
#endif

  size_t epotemp_length = TUPLE_NUM * sizeof(Tuple) / PER_XX_TEMP + 1;
  // cout << "eptmp_length:\t" << eptmp_length << endl;
  if (posix_memalign((void **)&EpotempAry, PAGE_SIZE,
                     epotemp_length * sizeof(Epotemp)) != 0)
    ERR;

  size_t maxthread = decideParallelBuildNumber(TUPLE_NUM);
  std::vector<std::thread> thv;
  for (size_t i = 0; i < maxthread; ++i) {
    thv.emplace_back(partTableInit, i, i * (TUPLE_NUM / maxthread),
                     (i + 1) * (TUPLE_NUM / maxthread) - 1);
  }
  for (auto &th : thv) th.join();
}

bool chkEpochLoaded() {
  uint64_t_64byte nowepo = loadAcquireGE();
  for (unsigned int i = 1; i < THREAD_NUM; ++i) {
    if (__atomic_load_n(&(ThLocalEpoch[i].obj_), __ATOMIC_ACQUIRE) !=
        nowepo.obj_)
      return false;
  }

  return true;
}

void leaderWork(uint64_t &epoch_timer_start, uint64_t &epoch_timer_stop,
                [[maybe_unused]] Result &res) {
  epoch_timer_stop = rdtscp();
  // chkEpochLoaded は最新のグローバルエポックを
  //全てのワーカースレッドが読み込んだか確認する．
  if (chkClkSpan(epoch_timer_start, epoch_timer_stop,
                 EPOCH_TIME * CLOCKS_PER_US * 1000) &&
      chkEpochLoaded()) {
    atomicAddGE();
    epoch_timer_start = epoch_timer_stop;

#if TEMPERATURE_RESET_OPT
#else
    size_t epotemp_length = TUPLE_NUM * sizeof(Tuple) / PER_XX_TEMP + 1;
    uint64_t nowepo = (loadAcquireGE()).obj;
    for (uint64_t i = 0; i < epotemp_length; ++i) {
      Epotemp epotemp(0, nowepo);
      storeRelease(EpotempAry[i].obj_, epotemp.obj_);
    }
#if ADD_ANALYSIS
    res.local_temperature_resets += epotemp_length;
#endif
#endif
  }
}
