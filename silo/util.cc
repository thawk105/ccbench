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
#include "include/util.hh"

#include "../include/cache_line_size.hh"
#include "../include/config.hh"
#include "../include/debug.hh"
#include "../include/masstree_wrapper.hh"
#include "../include/procedure.hh"
#include "../include/random.hh"
#include "../include/tsc.hh"
#include "../include/util.hh"
#include "../include/zipf.hh"

void chkArg() {
  displayParameter();

  TotalThreadNum = FLAGS_thread_num + FLAGS_batch_th_num;

  // TODO: enable this later
  // if (FLAGS_rratio > 100) {
  //   ERR;
  // }

  // if (FLAGS_zipf_skew >= 1) {
  //   cout << "FLAGS_zipf_skew must be 0 ~ 0.999..." << endl;
  //   ERR;
  // }

  // if (FLAGS_tuple_num < FLAGS_batch_tuples) {
  //   cout << "FLAGS_batch_tuples must be small than FLAGS_tuple_num" << endl;
  //   ERR;
  // }

  if (posix_memalign((void **) &ThLocalEpoch, CACHE_LINE_SIZE,
                     TotalThreadNum * sizeof(uint64_t_64byte)) != 0)
    ERR;
  if (posix_memalign((void **) &CTIDW, CACHE_LINE_SIZE,
                     TotalThreadNum * sizeof(uint64_t_64byte)) != 0)
    ERR;

  // init
  for (unsigned int i = 0; i < TotalThreadNum; ++i) {
    ThLocalEpoch[i].obj_ = 0;
    CTIDW[i].obj_ = 0;
  }
}

bool chkEpochLoaded() {
  uint64_t nowepo = atomicLoadGE();
  //全てのワーカースレッドが最新エポックを読み込んだか確認する．
  for (unsigned int i = 1; i < TotalThreadNum; ++i) {
    if (__atomic_load_n(&(ThLocalEpoch[i].obj_), __ATOMIC_ACQUIRE) != nowepo)
      return false;
  }

  return true;
}

// TODO: enable this if we want to use
// void displayDB() {
//   Tuple *tuple;
//   for (unsigned int i = 0; i < FLAGS_tuple_num; ++i) {
//     tuple = &Table[i];
//     cout << "------------------------------" << endl;  //-は30個
//     cout << "key: " << i << endl;
//     cout << "val: " << tuple->val_ << endl;
//     cout << "TIDword: " << tuple->tidword_.obj_ << endl;
//     cout << "bit: " << tuple->tidword_.obj_ << endl;
//     cout << endl;
//   }
// }

void displayParameter() {
  cout << "#FLAGS_clocks_per_us:\t" << FLAGS_clocks_per_us << endl;
  cout << "#FLAGS_epoch_time:\t" << FLAGS_epoch_time << endl;
  cout << "#FLAGS_extime:\t\t" << FLAGS_extime << endl;
  cout << "#FLAGS_thread_num:\t" << FLAGS_thread_num << endl;
  if (FLAGS_batch_th_num > 0 || FLAGS_batch_ratio > 0) {
    cout << "#FLAGS_batch_th_num:\t" << FLAGS_batch_th_num << endl;
    cout << "#FLAGS_batch_ratio:\t" << FLAGS_batch_ratio << endl;
    cout << "#FLAGS_batch_max_ope:\t" << FLAGS_batch_max_ope << endl;
    cout << "#FLAGS_batch_rratio:\t" << FLAGS_batch_rratio << endl;
    cout << "#FLAGS_batch_tuples:\t" << FLAGS_batch_tuples << endl;
    cout << "#FLAGS_batch_simple_rr:\t" << FLAGS_batch_simple_rr << endl;
  }
}

void genLogFile(std::string &logpath, const int thid) {
  genLogFileName(logpath, thid);
  createEmptyFile(logpath);
}

// void partTableInit([[maybe_unused]] size_t thid, uint64_t start, uint64_t end) {
// #if MASSTREE_USE
//   MasstreeWrapper<Tuple>::thread_init(thid);
// #endif

//   for (auto i = start; i <= end; ++i) {
//     Tuple *tmp;
//     tmp = &Table[i];
//     tmp->tidword_.epoch = 1;
//     tmp->tidword_.latest = 1;
//     tmp->tidword_.lock = 0;
//     tmp->val_[0] = 'a';
//     tmp->val_[1] = '\0';

// #if MASSTREE_USE
//     MT.insert_value(i, tmp);
// #endif
//   }
// }

// void DefaultInitializer::makeDB() {
//   if (posix_memalign((void **) &Table, PAGE_SIZE,
//                      (FLAGS_tuple_num) * sizeof(Tuple)) != 0)
//     ERR;
// #if dbs11
//   if (madvise((void *)Table, (FLAGS_tuple_num) * sizeof(Tuple),
//               MADV_HUGEPAGE) != 0)
//     ERR;
// #endif

//   size_t maxthread = decideParallelBuildNumber(FLAGS_tuple_num);

//   std::vector<std::thread> thv;
//   for (size_t i = 0; i < maxthread; ++i)
//     thv.emplace_back(partTableInit, i, i * (FLAGS_tuple_num / maxthread),
//                      (i + 1) * (FLAGS_tuple_num / maxthread) - 1);
//   for (auto &th : thv) th.join();
// }

void siloLeaderWork(uint64_t &epoch_timer_start, uint64_t &epoch_timer_stop) {
  epoch_timer_stop = rdtscp();
  if (chkClkSpan(epoch_timer_start, epoch_timer_stop,
                 FLAGS_epoch_time * FLAGS_clocks_per_us * 1000) &&
      chkEpochLoaded()) {
    atomicAddGE();
    uint32_t cur_epoch = atomicLoadGE();
    ReclamationEpoch = cur_epoch > 2 ? cur_epoch - 2 : 0;
    epoch_timer_start = epoch_timer_stop;
  }
}

void ShowOptParameters() {
  cout << "#ShowOptParameters()"
       << ": ADD_ANALYSIS " << ADD_ANALYSIS << ": BACK_OFF " << BACK_OFF
       << ": KEY_SIZE " << KEY_SIZE << ": MASSTREE_USE " << MASSTREE_USE
       << ": NO_WAIT_LOCKING_IN_VALIDATION " << NO_WAIT_LOCKING_IN_VALIDATION
       << ": PARTITION_TABLE " << PARTITION_TABLE << ": PROCEDURE_SORT "
       << PROCEDURE_SORT << ": SLEEP_READ_PHASE " << SLEEP_READ_PHASE
       << ": VAL_SIZE " << VAL_SIZE << ": WAL " << WAL
#ifdef INSERT_READ_DELAY_MS
       << ": INSERT_READ_DELAY_MS " << INSERT_READ_DELAY_MS
#endif
#ifdef INSERT_BATCH_DELAY_MS
       << ": INSERT_BATCH_DELAY_MS " << INSERT_BATCH_DELAY_MS
#endif
       << endl;
}
