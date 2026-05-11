#include <stdint.h>
#include <stdlib.h>
#include <atomic>
#include <bitset>
#include <iostream>

#include "../include/backoff.hh"
#include "../include/masstree_wrapper.hh"
#include "include/common.hh"
#include "include/util.hh"

using std::cout, std::endl;

void chkArg() {
  displayParameter();

  TotalThreadNum = FLAGS_thread_num;

  if (FLAGS_clocks_per_us < 100) {
    printf("CPU_MHZ is less than 100. are you really?\n");
    exit(0);
  }

  if (posix_memalign((void **) &ThreadRtsArrayForGroup, CACHE_LINE_SIZE,
                     TotalThreadNum * sizeof(uint64_t_64byte)) != 0)
    ERR;
  if (posix_memalign((void **) &ThreadWtsArray, CACHE_LINE_SIZE,
                     TotalThreadNum * sizeof(uint64_t_64byte)) != 0)
    ERR;
  if (posix_memalign((void **) &ThreadRtsArray, CACHE_LINE_SIZE,
                     TotalThreadNum * sizeof(uint64_t_64byte)) != 0)
    ERR;
  if (posix_memalign((void **) &GROUP_COMMIT_INDEX, CACHE_LINE_SIZE,
                     TotalThreadNum * sizeof(uint64_t_64byte)) != 0)
    ERR;
  if (posix_memalign((void **) &GROUP_COMMIT_COUNTER, CACHE_LINE_SIZE,
                     TotalThreadNum * sizeof(uint64_t_64byte)) != 0)
    ERR;
  if (posix_memalign((void **) &GCFlag, CACHE_LINE_SIZE,
                     TotalThreadNum * sizeof(uint64_t_64byte)) != 0)
    ERR;
  if (posix_memalign((void **) &GCExecuteFlag, CACHE_LINE_SIZE,
                     TotalThreadNum * sizeof(uint64_t_64byte)) != 0)
    ERR;

  // init
  for (unsigned int i = 0; i < TotalThreadNum; ++i) {
    GCFlag[i].obj_ = 0;
    GCExecuteFlag[i].obj_ = 0;
    ThreadRtsArray[i].obj_ = 0;
    ThreadWtsArray[i].obj_ = 0;
    ThreadRtsArrayForGroup[i].obj_ = 0;
  }
}

void displayMinRts() { cout << "MinRts:  " << MinRts << endl << endl; }

void displayMinWts() { cout << "MinWts:  " << MinWts << endl << endl; }

void displayParameter() {
  cout << "#FLAGS_clocks_per_us:\t\t\t" << FLAGS_clocks_per_us << endl;
  cout << "#FLAGS_extime:\t\t\t\t" << FLAGS_extime << endl;
  cout << "#FLAGS_gc_inter_us:\t\t\t" << FLAGS_gc_inter_us << endl;
  cout << "#FLAGS_io_time_ns:\t\t\t" << FLAGS_io_time_ns << endl;
  cout << "#FLAGS_thread_num:\t\t\t" << FLAGS_thread_num << endl;
}

void displayThreadWtsArray() {
  cout << "ThreadWtsArray:" << endl;
  for (unsigned int i = 0; i < TotalThreadNum; i++) {
    cout << "thid " << i << ": " << ThreadWtsArray[i].obj_ << endl;
  }
  cout << endl << endl;
}

void displayThreadRtsArray() {
  cout << "ThreadRtsArray:" << endl;
  for (unsigned int i = 0; i < TotalThreadNum; i++) {
    cout << "thid " << i << ": " << ThreadRtsArray[i].obj_ << endl;
  }
  cout << endl << endl;
}

void mvtoLeaderWork() {
  bool gc_update = true;
  for (unsigned int i = 0; i < TotalThreadNum; ++i) {
    // check all thread's flag raising
    if (__atomic_load_n(&(GCFlag[i].obj_), __ATOMIC_ACQUIRE) == 0) {
      gc_update = false;
      break;
    }
  }
  if (gc_update) {
    uint64_t minw =
            __atomic_load_n(&(ThreadWtsArray[0].obj_), __ATOMIC_ACQUIRE);
    uint64_t minr;
    minr = __atomic_load_n(&(ThreadRtsArray[0].obj_), __ATOMIC_ACQUIRE);

    for (unsigned int i = 1; i < TotalThreadNum; ++i) {
      uint64_t tmp =
              __atomic_load_n(&(ThreadWtsArray[i].obj_), __ATOMIC_ACQUIRE);
      if (minw > tmp) minw = tmp;
      tmp = __atomic_load_n(&(ThreadRtsArray[i].obj_), __ATOMIC_ACQUIRE);
      if (minr > tmp) minr = tmp;
    }

    MinWts.store(minw, memory_order_release);
    MinRts.store(minr, memory_order_release);

    // downgrade gc flag
    for (unsigned int i = 0; i < TotalThreadNum; ++i) {
      __atomic_store_n(&(GCFlag[i].obj_), 0, __ATOMIC_RELEASE);
      __atomic_store_n(&(GCExecuteFlag[i].obj_), 1, __ATOMIC_RELEASE);
    }
  }
}

void ShowOptParameters() {
  cout << "#ShowOptParameters()"
       << ": ADD_ANALYSIS " << ADD_ANALYSIS
       << ": BACK_OFF " << BACK_OFF
       << ": MASSTREE_USE " << MASSTREE_USE
       << ": KEY_SIZE " << KEY_SIZE
       << ": VAL_SIZE " << VAL_SIZE
       << endl;
}
