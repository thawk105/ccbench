
#include <stdlib.h>
#include <sys/syscall.h>  // syscall(SYS_gettid),
#include <sys/types.h>    // syscall(SSY_gettid),
#include <unistd.h>       // syscall(SSY_gettid),

#include <atomic>
#include <bitset>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <thread>
#include <type_traits>
#include <vector>

#include "../include/check.hh"
#include "../include/config.hh"
#include "../include/debug.hh"
#include "../include/masstree_wrapper.hh"
#include "../include/procedure.hh"
#include "../include/random.hh"
#include "../include/result.hh"
#include "../include/zipf.hh"
#include "include/common.hh"
#include "include/tuple.hh"

extern size_t decideParallelBuildNumber(size_t tuplenum);

void chkArg(const int argc, const char *argv[]) {
  if (argc != 10) {
    cout << "usage: ./ss2pl.exe TUPLE_NUM MAX_OPE THREAD_NUM RRATIO RMW "
            "ZIPF_SKEW YCSB CPU_MHZ EXTIME"
         << endl
         << endl;
    cout << "example: ./ss2pl.exe 200 10 24 50 off 0 on 2100 3" << endl << endl;
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
    cout << "EXTIME: execution time [sec]" << endl << endl;

    cout << "Tuple size " << sizeof(Tuple) << endl;
    cout << "std::mutex size " << sizeof(mutex) << endl;
    cout << "RWLock size " << sizeof(RWLock) << endl;
    cout << "KEY_SIZE : " << KEY_SIZE << endl;
    cout << "VAL_SIZE : " << VAL_SIZE << endl;
    cout << "std::thread::hardware_concurrency()="
         << std::thread::hardware_concurrency() << endl;
    cout << "Procedure : is_move_constructible : "
         << std::is_move_constructible<Procedure>::value << endl;
    cout << "Procedure : is_move_assignable : "
         << std::is_move_assignable<Procedure>::value << endl;
    cout << "Result size " << sizeof(Result) << endl;
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

  if (CLOCKS_PER_US < 100) {
    cout << "CPU_MHZ is less than 100. are your really?" << endl;
    ERR;
  }
}

void displayDB() {
  Tuple *tuple;

  for (unsigned int i = 0; i < TUPLE_NUM; i++) {
    tuple = &Table[i];
    cout << "------------------------------" << endl;  // - 30
    cout << "key: " << i << endl;
    cout << "val: " << tuple->val_ << endl;
  }
}

void partTableInit([[maybe_unused]] size_t thid, uint64_t start, uint64_t end) {
  // printf("partTableInit(...): thid %zu : %lu : %lu\n", thid, start, end);
#if MASSTREE_USE
  MasstreeWrapper<Tuple>::thread_init(thid);
#endif

  for (auto i = start; i <= end; ++i) {
    Table[i].val_[0] = 'a';
    Table[i].val_[1] = '\0';

#if MASSTREE_USE
    MT.insert_value(i, &Table[i]);
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

  // maxthread は masstree 構築の最大並行スレッド数。
  // 初期値はハードウェア最大値。
  // TUPLE_NUM を均等に分割できる最大スレッド数を求める。
  size_t maxthread = decideParallelBuildNumber(TUPLE_NUM);

  std::vector<std::thread> thv;
  // cout << "masstree 並列構築スレッド数 " << maxthread << endl;
  for (size_t i = 0; i < maxthread; ++i) {
    thv.emplace_back(partTableInit, i, i * (TUPLE_NUM / maxthread),
                     (i + 1) * (TUPLE_NUM / maxthread) - 1);
  }
  for (auto &th : thv) th.join();
}
