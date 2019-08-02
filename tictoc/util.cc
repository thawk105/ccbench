
#include <sys/syscall.h>  // syscall(SYS_gettid),
#include <sys/time.h>
#include <sys/types.h>  // syscall(SYS_gettid),
#include <unistd.h>     // syscall(SYS_gettid),

#include <atomic>
#include <bitset>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <thread>
#include <vector>

#include "../include/check.hh"
#include "../include/config.hh"
#include "../include/debug.hh"
#include "../include/inline.hh"
#include "../include/random.hh"
#include "../include/result.hh"
#include "../include/zipf.hh"
#include "include/common.hh"
#include "include/transaction.hh"
#include "include/tuple.hh"

using namespace std;

extern size_t decideParallelBuildNumber(size_t tuplenum);

void chkArg(const int argc, char *argv[]) {
  if (argc != 10) {
    cout << "usage:./main TUPLE_NUM MAX_OPE THREAD_NUM RRATIO RMW ZIPF_SKEW "
            "YCSB CLOCKS_PER_US EXTIME"
         << endl
         << endl;

    cout << "example:./main 200 10 24 50 off 0 on 2100 3" << endl << endl;

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
    cout << "EXTIME: execution time." << endl << endl;

    cout << "Tuple " << sizeof(Tuple) << endl;
    cout << "uint64_t_64byte " << sizeof(uint64_t_64byte) << endl;
    cout << "KEY_SIZE : " << KEY_SIZE << endl;
    cout << "VAL_SIZE : " << VAL_SIZE << endl;
    cout << "NO_WAIT_LOCKING_IN_VALIDATION: " << NO_WAIT_LOCKING_IN_VALIDATION
         << endl;
    cout << "PREEMPTIVE_ABORTS: " << PREEMPTIVE_ABORTS << endl;
    cout << "TIMESTAMP_HISTORY: " << TIMESTAMP_HISTORY << endl;
    cout << "MASSTREE_USE : " << MASSTREE_USE << endl;
    cout << "Result " << sizeof(Result) << endl;
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
  CLOCKS_PER_US = atof(argv[8]);
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

void displayDB() {
  Tuple *tuple;

  for (unsigned int i = 0; i < TUPLE_NUM; ++i) {
    tuple = &Table[i];
    cout << "------------------------------" << endl;  //-は30個
    cout << "key: " << i << endl;
    cout << "val_: " << tuple->val_ << endl;
    cout << "TS_word: " << tuple->tsw_.obj_ << endl;
    cout << "bit: " << static_cast<bitset<64>>(tuple->tsw_.obj_) << endl;
    cout << endl;
  }
}

void partTableInit([[maybe_unused]] size_t thid, uint64_t start, uint64_t end) {
#if MASSTREE_USE
  MasstreeWrapper<Tuple>::thread_init(thid);
#endif

  for (auto i = start; i <= end; ++i) {
    Tuple *tmp = &Table[i];
    tmp->tsw_.obj_ = 0;
    tmp->pre_tsw_.obj_ = 0;
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
