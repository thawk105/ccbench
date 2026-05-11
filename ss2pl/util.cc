
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

  TotalThreadNum = FLAGS_thread_num;

  if (FLAGS_clocks_per_us < 100) {
    cout << "CPU_MHZ is less than 100. are your really?" << endl;
    ERR;
  }
}

void displayDB() {}

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

void partTableInit([[maybe_unused]] size_t thid, uint64_t start, uint64_t end) {}

void makeDB() {}

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
