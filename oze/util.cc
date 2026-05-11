#include <stdint.h>
#include <stdlib.h>
#include <atomic>
#include <bitset>
#include <iostream>

#include "../include/backoff.hh"

#include "include/atomic_tool.hh"
#include "include/common.hh"
#include "include/util.hh"

using std::cout, std::endl;

void init(int32_t table_num) {
  displayParameter();

  TotalThreadNum = FLAGS_thread_num;

  if (FLAGS_clocks_per_us < 100) {
    printf("CPU_MHZ is less than 100. are you really?\n");
    exit(0);
  }

  // Prepare two (even/odd) scan histories for each table
  // offset = 2 * storage_index + epoch % 2
  if (posix_memalign((void **) &ScanHistory, CACHE_LINE_SIZE,
                     table_num * 2 * sizeof(atomic<ScanEntry*>)) != 0)
    ERR;
  if (posix_memalign((void **) &ScanRange, CACHE_LINE_SIZE,
                     table_num * 2 * sizeof(atomic<uint64_t>)) != 0)
    ERR;

  // init
  try {
    ThManagementTable = new ThreadManagementEntry *[TotalThreadNum];
  } catch (bad_alloc) {
    ERR;
  }
  for (unsigned int i = 0; i < TotalThreadNum; ++i) {
    ThManagementTable[i] = new ThreadManagementEntry(1, FLAGS_cc_mode);
  }
  for (unsigned int i = 0; i < table_num * 2; ++i) {
    ScanHistory[i].store(nullptr);
  }
}

void displayParameter() {
  cout << "#FLAGS_clocks_per_us:\t" << FLAGS_clocks_per_us << endl;
  cout << "#FLAGS_extime:\t\t" << FLAGS_extime << endl;
  cout << "#FLAGS_epoch_time:\t" << FLAGS_epoch_time << endl;
  cout << "#FLAGS_io_time_ns:\t" << FLAGS_io_time_ns << endl;
  cout << "#FLAGS_thread_num:\t" << FLAGS_thread_num << endl;
  cout << "#FLAGS_forwarding:\t" << FLAGS_forwarding << endl;
  if (FLAGS_validation_th_num > 1) {
    cout << "#FLAGS_validation_th_num:\t" << FLAGS_validation_th_num << endl;
    cout << "#FLAGS_validation_threshold:\t" << FLAGS_validation_threshold << endl;
  }
}

bool chkEpochLoaded() {
  uint64_t nowepo = atomicLoadGE();
  // check if all worker threads read the latest epoch
  for (unsigned int i = 0; i < TotalThreadNum; ++i) {
    if (ThManagementTable[i]->atomicLoadEpoch() != nowepo)
      return false;
  }

  return true;
}

void ozeLeaderWork(uint64_t &epoch_timer_start, uint64_t &epoch_timer_stop) {
  epoch_timer_stop = rdtscp();
  if (chkClkSpan(epoch_timer_start, epoch_timer_stop,
                 FLAGS_epoch_time * FLAGS_clocks_per_us * 1000) &&
      chkEpochLoaded()) {
    atomicAddGE();
#if DEBUG_MSG
    std::cout << "New epoch: " << atomicLoadGE() << std::endl;
#endif
    epoch_timer_start = epoch_timer_stop;
  }
}

void ShowOptParameters() {
  cout << "#ShowOptParameters()"
       << ": ADD_ANALYSIS " << ADD_ANALYSIS << ": BACK_OFF " << BACK_OFF
       << ": MASSTREE_USE " << MASSTREE_USE << ": PARTITION_TABLE "
       << PARTITION_TABLE << ": REUSE_VERSION " << REUSE_VERSION
       << ": SINGLE_EXEC " << SINGLE_EXEC << ": KEY_SIZE " << KEY_SIZE
       << ": VAL_SIZE " << VAL_SIZE << ": WRITE_LATEST_ONLY "
       << WRITE_LATEST_ONLY
#ifdef INSERT_READ_DELAY_MS
       << ": INSERT_READ_DELAY_MS " << INSERT_READ_DELAY_MS
#endif
#ifdef INSERT_BATCH_DELAY_MS
       << ": INSERT_BATCH_DELAY_MS " << INSERT_BATCH_DELAY_MS
#endif
       << endl;
}
