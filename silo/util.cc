#include <stdio.h>
#include <sys/syscall.h>  // syscall(SYS_gettid),
#include <sys/time.h>
#include <sys/types.h>  // syscall(SYS_gettid),
#include <unistd.h>     // syscall(SYS_gettid),

#include <bitset>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <thread>
#include <vector>

#include "include/atomic_tool.hh"
#include "include/common.hh"
#include "include/transaction.hh"
#include "include/tuple.hh"

#include "../include/cache_line_size.hh"
#include "../include/check.hh"
#include "../include/config.hh"
#include "../include/debug.hh"
#include "../include/masstree_wrapper.hh"
#include "../include/procedure.hh"
#include "../include/random.hh"
#include "../include/tsc.hh"
#include "../include/zipf.hh"

extern size_t decideParallelBuildNumber(size_t tuplenum);

void chkArg(const int argc, char *argv[]) {
  if (argc != 11) {
    cout << "usage:./main TUPLE_NUM MAX_OPE THREAD_NUM RRATIO RMW ZIPF_SKEW "
            "YCSB CLOCKS_PER_US EPOCH_TIME EXTIME"
         << endl
         << endl;

    cout << "example:./main 1000000 10 24 50 off 0 on 2100 40 3" << endl
         << endl;
    cout << "TUPLE_NUM(int): total numbers of sets of key-value (1, 100), (2, "
            "100)"
         << endl;
    cout << "MAX_OPE(int):    total numbers of operations" << endl;
    cout << "THREAD_NUM(int): total numbers of thread." << endl;
    cout << "RRATIO : read ratio [%%]" << endl;
    cout << "RMW : read modify write. on or off." << endl;
    cout << "ZIPF_SKEW : zipf skew. 0 ~ 0.999..." << endl;
    cout << "YCSB : on or off. switch makeProcedure function." << endl;
    cout << "CLOCKS_PER_US: CPU_MHZ" << endl;
    cout << "EPOCH_TIME(int)(ms): Ex. 40" << endl;
    cout << "EXTIME: execution time." << endl << endl;
    cout << "Tuple " << sizeof(Tuple) << endl;
    cout << "uint64_t_64byte " << sizeof(uint64_t_64byte) << endl;
    cout << "Tidword " << sizeof(Tidword) << endl;
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
  EXTIME = atoi(argv[10]);

  if (RRATIO > 100) {
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

  if (argycsb == "on")
    YCSB = true;
  else if (argycsb == "off")
    YCSB = false;
  else
    ERR;

  if (THREAD_NUM < 2) {
    printf(
        "One thread is epoch thread, and others are worker threads.\n\
So you have to set THREAD_NUM >= 2.\n\n");
  }

  if (posix_memalign((void **)&ThLocalEpoch, CACHE_LINE_SIZE,
                     THREAD_NUM * sizeof(uint64_t_64byte)) != 0)
    ERR;  //[0]は使わない
  if (posix_memalign((void **)&CTIDW, CACHE_LINE_SIZE,
                     THREAD_NUM * sizeof(uint64_t_64byte)) != 0)
    ERR;  //[0]は使わない
  // init
  for (unsigned int i = 0; i < THREAD_NUM; ++i) {
    ThLocalEpoch[i].obj_ = 0;
    CTIDW[i].obj_ = 0;
  }
}

bool chkEpochLoaded() {
  uint64_t nowepo = atomicLoadGE();
  //全てのワーカースレッドが最新エポックを読み込んだか確認する．
  for (unsigned int i = 1; i < THREAD_NUM; ++i) {
    if (__atomic_load_n(&(ThLocalEpoch[i].obj_), __ATOMIC_ACQUIRE) != nowepo)
      return false;
  }

  return true;
}

void displayDB() {
  Tuple *tuple;
  for (unsigned int i = 0; i < TUPLE_NUM; ++i) {
    tuple = &Table[i];
    cout << "------------------------------" << endl;  //-は30個
    cout << "key: " << i << endl;
    cout << "val: " << tuple->val_ << endl;
    cout << "TIDword: " << tuple->tidword_.obj_ << endl;
    cout << "bit: " << tuple->tidword_.obj_ << endl;
    cout << endl;
  }
}

void displayParameter() {
  cout << "TUPLE_NUM:\t" << TUPLE_NUM << endl;
  cout << "MAX_OPE:\t" << MAX_OPE << endl;
  cout << "THREAD_NUM:\t" << THREAD_NUM << endl;
  cout << "RRATIO:\t" << RRATIO << endl;
  cout << "RMW:\t" << RMW << endl;
  cout << "ZIPF_SKEW:\t" << ZIPF_SKEW << endl;
  cout << "YCSB:\t" << YCSB << endl;
  cout << "CLOCKS_PER_US:\t" << CLOCKS_PER_US << endl;
  cout << "EPOCH_TIME:\t" << EPOCH_TIME << endl;
  cout << "EXTIME:\t" << EXTIME << endl;
}

void genLogFile(std::string &logpath, const int thid) {
  genLogFileName(logpath, thid);
  createEmptyFile(logpath);
}

void partTableInit([[maybe_unused]] size_t thid, uint64_t start, uint64_t end) {
#if MASSTREE_USE
  MasstreeWrapper<Tuple>::thread_init(thid);
#endif

  for (auto i = start; i <= end; ++i) {
    Tuple *tmp;
    tmp = &Table[i];
    tmp->tidword_.epoch = 1;
    tmp->tidword_.latest = 1;
    tmp->tidword_.lock = 0;
    tmp->val_[0] = 'a';
    tmp->val_[1] = '\0';

#if MASSTREE_USE
    MT.insert_value(i, tmp);
#endif
  }
}

void makeDB() {
  if (posix_memalign((void **)&Table, PAGE_SIZE, (TUPLE_NUM) * sizeof(Tuple)) !=
      0)
    ERR;
#if dbs11
  if (madvise((void *)Table, (TUPLE_NUM) * sizeof(Tuple), MADV_HUGEPAGE) != 0)
    ERR;
#endif

  size_t maxthread = decideParallelBuildNumber(TUPLE_NUM);

  std::vector<std::thread> thv;
  for (size_t i = 0; i < maxthread; ++i)
    thv.emplace_back(partTableInit, i, i * (TUPLE_NUM / maxthread),
                     (i + 1) * (TUPLE_NUM / maxthread) - 1);
  for (auto &th : thv) th.join();
}

void leaderWork(uint64_t &epoch_timer_start, uint64_t &epoch_timer_stop) {
  epoch_timer_stop = rdtscp();
  if (chkClkSpan(epoch_timer_start, epoch_timer_stop,
                 EPOCH_TIME * CLOCKS_PER_US * 1000) &&
      chkEpochLoaded()) {
    atomicAddGE();
    epoch_timer_start = epoch_timer_stop;
  }
}
