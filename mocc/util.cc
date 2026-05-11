
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
#include "include/util.hh"

using namespace std;

void chkArg() {
  displayParameter();

  TotalThreadNum = FLAGS_thread_num + FLAGS_batch_th_num;

  if (FLAGS_rratio > 100) {
    cout << "rratio (* 10 \%) must be 0 ~ 10)" << endl;
    ERR;
  }

  if (FLAGS_zipf_skew >= 1) {
    cout << "FLAGS_zipf_skew must be 0 ~ 0.999..." << endl;
    ERR;
  }

  if (FLAGS_clocks_per_us < 100) {
    cout << "CPU_MHZ is less than 100. are your really?" << endl;
    ERR;
  }

  if (FLAGS_per_xx_temp < sizeof(Tuple)) {
    cout << "FLAGS_per_xx_temp's minimum is sizeof(Tuple) " << sizeof(Tuple) << endl;
    ERR;
  }

  if (FLAGS_tuple_num < FLAGS_batch_tuples) {
    cout << "FLAGS_batch_tuples must be small than FLAGS_tuple_num" << endl;
    ERR;
  }

  if (posix_memalign((void **) &Start, 64,
                     TotalThreadNum * sizeof(uint64_t_64byte)) != 0)
    ERR;
  if (posix_memalign((void **) &Stop, 64,
                     TotalThreadNum * sizeof(uint64_t_64byte)) != 0)
    ERR;
  if (posix_memalign((void **) &ThLocalEpoch, 64,
                     TotalThreadNum * sizeof(uint64_t_64byte)) != 0)
    ERR;
#ifdef MQLOCK
  // if (posix_memalign((void**)&MQLNodeList, 64, (TotalThreadNum + 3) *
  // sizeof(MQLNode)) != 0) ERR;
  MQLNodeTable = new MQLNode *[TotalThreadNum + 3];
  for (unsigned int i = 0; i < TotalThreadNum + 3; ++i)
    MQLNodeTable[i] = new MQLNode[TotalThreadNum];
#endif  // MQLOCK

  for (unsigned int i = 0; i < TotalThreadNum; ++i) {
    ThLocalEpoch[i].obj_ = 0;
  }
}

// TODO: enable this if we want to use
// void displayDB() {
//   Tuple *tuple;

//   for (unsigned int i = 0; i < FLAGS_tuple_num; ++i) {
//     tuple = &Table[i];
//     cout << "----------" << endl;  // - is 10
//     cout << "key: " << i << endl;
//     cout << "val: " << tuple->val_ << endl;
//     cout << "lockctr: " << tuple->rwlock_.counter_ << endl;
//     cout << "TIDword: " << tuple->tidword_.obj_ << endl;
//     cout << "bit: " << static_cast<bitset<64>>(tuple->tidword_.obj_) << endl;
//     cout << endl;
//   }
// }

// TODO: enable this if we want to use
// void displayLockedTuple() {
//   for (unsigned int i = 0; i < FLAGS_tuple_num; ++i) {
// #ifdef RWLOCK
//     if (Table[i].rwlock_.counter_.load(memory_order_relaxed) == -1) {
// #endif  // RWLOCK
//     cout << "key : " << i << " is locked!." << endl;
// #ifdef RWLOCK
//     }
// #endif
//   }
// }

void displayParameter() {
  cout << "#FLAGS_clocks_per_us:\t" << FLAGS_clocks_per_us << endl;
  cout << "#FLAGS_epoch_time:\t" << FLAGS_epoch_time << endl;
  cout << "#FLAGS_extime:\t\t" << FLAGS_extime << endl;
  cout << "#FLAGS_max_ope:\t\t" << FLAGS_max_ope << endl;
  cout << "#FLAGS_per_xx_temp\t" << FLAGS_per_xx_temp << endl;
  cout << "#FLAGS_temp_threshold\t" << FLAGS_temp_threshold << endl;
  cout << "#FLAGS_rmw:\t\t" << FLAGS_rmw << endl;
  cout << "#FLAGS_rratio:\t\t" << FLAGS_rratio << endl;
  cout << "#FLAGS_thread_num:\t" << FLAGS_thread_num << endl;
  cout << "#FLAGS_tuple_num:\t" << FLAGS_tuple_num << endl;
  cout << "#FLAGS_ycsb:\t\t" << FLAGS_ycsb << endl;
  cout << "#FLAGS_zipf_skew:\t" << FLAGS_zipf_skew << endl;
  if (FLAGS_batch_th_num > 0 || FLAGS_batch_ratio > 0) {
    cout << "#FLAGS_batch_th_num:\t" << FLAGS_batch_th_num << endl;
    cout << "#FLAGS_batch_ratio:\t" << FLAGS_batch_ratio << endl;
    cout << "#FLAGS_batch_max_ope:\t" << FLAGS_batch_max_ope << endl;
    cout << "#FLAGS_batch_rratio:\t" << FLAGS_batch_rratio << endl;
    cout << "#FLAGS_batch_tuples:\t" << FLAGS_batch_tuples << endl;
    cout << "#FLAGS_batch_simple_rr:\t" << FLAGS_batch_simple_rr << endl;
  }
}

// void partTableInit([[maybe_unused]] size_t thid, uint64_t start, uint64_t end) {
// #if MASSTREE_USE
//   MasstreeWrapper<Tuple>::thread_init(thid);
// #endif

//   for (uint64_t i = start; i <= end; ++i) {
//     Tuple *tmp;
//     tmp = &Table[i];
//     tmp->tidword_.epoch = 1;
//     tmp->tidword_.tid = 0;
//     tmp->val_[0] = 'a';
//     tmp->val_[1] = '\0';

//     Epotemp epotemp(0, 1);
//     size_t epotemp_index = i * sizeof(Tuple) / FLAGS_per_xx_temp;
//     // cout << "key:\t" << i << ", ep_index:\t" << ep_index << endl;
//     storeRelease(EpotempAry[epotemp_index].obj_, epotemp.obj_);
// #if MASSTREE_USE
//     MT.insert_value(i, tmp);
// #endif
//   }
// }

// void makeDB() {
//   if (posix_memalign((void **) &Table, PAGE_SIZE, FLAGS_tuple_num * sizeof(Tuple)) !=
//       0)
//     ERR;
// #if dbs11
//   if (madvise((void *)Table, (FLAGS_tuple_num) * sizeof(Tuple), MADV_HUGEPAGE) != 0)
//     ERR;
// #endif

//   size_t epotemp_length = FLAGS_tuple_num * sizeof(Tuple) / FLAGS_per_xx_temp + 1;
//   // cout << "eptmp_length:\t" << eptmp_length << endl;
//   if (posix_memalign((void **) &EpotempAry, PAGE_SIZE,
//                      epotemp_length * sizeof(Epotemp)) != 0)
//     ERR;

//   size_t maxthread = decideParallelBuildNumber(FLAGS_tuple_num);
//   std::vector<std::thread> thv;
//   for (size_t i = 0; i < maxthread; ++i) {
//     thv.emplace_back(partTableInit, i, i * (FLAGS_tuple_num / maxthread),
//                      (i + 1) * (FLAGS_tuple_num / maxthread) - 1);
//   }
//   for (auto &th : thv) th.join();
// }

bool chkEpochLoaded() {
  uint64_t_64byte nowepo = loadAcquireGE();
  for (unsigned int i = 1; i < TotalThreadNum; ++i) {
    if (__atomic_load_n(&(ThLocalEpoch[i].obj_), __ATOMIC_ACQUIRE) !=
        nowepo.obj_)
      return false;
  }

  return true;
}

void moccLeaderWork(uint64_t &epoch_timer_start, uint64_t &epoch_timer_stop) {
  epoch_timer_stop = rdtscp();
  // chkEpochLoaded は最新のグローバルエポックを
  //全てのワーカースレッドが読み込んだか確認する．
  if (chkClkSpan(epoch_timer_start, epoch_timer_stop,
                 FLAGS_epoch_time * FLAGS_clocks_per_us * 1000) &&
      chkEpochLoaded()) {
    atomicAddGE();
    uint32_t cur_epoch = atomicLoadGE();
    ReclamationEpoch = cur_epoch > 2 ? cur_epoch - 2 : 0;
    epoch_timer_start = epoch_timer_stop;

#if TEMPERATURE_RESET_OPT
#else
    size_t epotemp_length = FLAGS_tuple_num * sizeof(Tuple) / FLAGS_per_xx_temp + 1;
    uint64_t nowepo = (loadAcquireGE()).obj_;
    for (uint64_t i = 0; i < epotemp_length; ++i) {
      Epotemp epotemp(0, nowepo);
      storeRelease(EpotempAry[i].obj_, epotemp.obj_);
    }
#if ADD_ANALYSIS
    res.local_temperature_resets_ += epotemp_length;
#endif
#endif
  }
}

void ShowOptParameters() {
  cout << "#ShowOptParameters() "
       << ": ADD_ANALYSIS " << ADD_ANALYSIS << ": BACK_OFF " << BACK_OFF
       << ": MASSTREE_USE " << MASSTREE_USE << ": KEY_SIZE " << KEY_SIZE
       << ": KEY_SORT " << KEY_SORT << ": TEMPERATURE_RESET_OPT "
       << TEMPERATURE_RESET_OPT << ": VAL_SIZE " << VAL_SIZE
#ifdef INSERT_READ_DELAY_MS
       << ": INSERT_READ_DELAY_MS " << INSERT_READ_DELAY_MS
#endif
#ifdef INSERT_BATCH_DELAY_MS
       << ": INSERT_BATCH_DELAY_MS " << INSERT_BATCH_DELAY_MS
#endif
       << endl;
}
