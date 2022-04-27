
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

#include "../include/config.hh"
#include "../include/debug.hh"
#include "../include/masstree_wrapper.hh"
#include "../include/procedure.hh"
#include "../include/random.hh"
#include "../include/result.hh"
#include "../include/zipf.hh"
#include "include/common.hh"
#include "include/tuple.hh"
#include "include/util.hh"

void chkArg() {
  displayParameter();

  if (FLAGS_rratio > 100) {
    ERR;
  }

  if (FLAGS_clocks_per_us < 100) {
    cout << "CPU_MHZ is less than 100. are your really?" << endl;
    ERR;
  }
}

void displayDB() {
  Tuple *tuple;

  for (unsigned int i = 0; i < FLAGS_tuple_num; i++) {
    tuple = &Table[i];
    cout << "------------------------------" << endl;  // - 30
    cout << "key: " << i << endl;
    cout << "val: " << tuple->val_ << endl;
  }
}

void displayParameter() {
  cout << "#FLAGS_clocks_per_us:\t" << FLAGS_clocks_per_us << endl;
  cout << "#FLAGS_extime:\t\t" << FLAGS_extime << endl;
  cout << "#FLAGS_max_ope:\t\t" << FLAGS_max_ope << endl;
  cout << "#FLAGS_rmw:\t\t" << FLAGS_rmw << endl;
  cout << "#FLAGS_rratio:\t\t" << FLAGS_rratio << endl;
  cout << "#FLAGS_thread_num:\t" << FLAGS_thread_num << endl;
  cout << "#FLAGS_tuple_num:\t" << FLAGS_tuple_num << endl;
  cout << "#FLAGS_ycsb:\t\t" << FLAGS_ycsb << endl;
  cout << "#FLAGS_zipf_skew:\t" << FLAGS_zipf_skew << endl;
}

void partTableInit([[maybe_unused]] size_t thid, uint64_t start, uint64_t end) {
  // printf("partTableInit(...): thid %zu : %lu : %lu\n", thid, start, end);
#if MASSTREE_USE
  MasstreeWrapper<Tuple>::thread_init(thid);
#endif

  for (auto i = start; i <= end; ++i) {
    Table[i].val_[0] = 'a';
    Table[i].val_[1] = '\0';
    Table[i].lock_.init();
#if MASSTREE_USE
    MT.insert_value(i, &Table[i]);
#endif
  }
}

void makeDB() {
  if (posix_memalign((void **) &Table, PAGE_SIZE, FLAGS_tuple_num * sizeof(Tuple)) !=
      0)
    ERR;
#if dbs11
  if (madvise((void *)Table, (FLAGS_tuple_num) * sizeof(Tuple), MADV_HUGEPAGE) != 0)
    ERR;
#endif

  // maxthread は masstree 構築の最大並行スレッド数。
  // 初期値はハードウェア最大値。
  // FLAGS_tuple_num を均等に分割できる最大スレッド数を求める。
  size_t maxthread = decideParallelBuildNumber(FLAGS_tuple_num);

  std::vector<std::thread> thv;
  // cout << "masstree 並列構築スレッド数 " << maxthread << endl;
  for (size_t i = 0; i < maxthread; ++i) {
    thv.emplace_back(partTableInit, i, i * (FLAGS_tuple_num / maxthread),
                     (i + 1) * (FLAGS_tuple_num / maxthread) - 1);
  }
  for (auto &th : thv) th.join();
}

void
ShowOptParameters() {
  cout << "#ShowOptParameters()"
       << ": ADD_ANALYSIS " << ADD_ANALYSIS
       << ": BACK_OFF " << BACK_OFF
       #ifdef DLR0
       << ": DLR0 "
       #elif defined DLR1
       << ": DLR1 "
       #endif
       << ": MASSTREE_USE " << MASSTREE_USE
       << ": KEY_SIZE " << KEY_SIZE
       << ": KEY_SORT " << KEY_SORT
       << ": VAL_SIZE " << VAL_SIZE
       << endl;
}
