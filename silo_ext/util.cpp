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
//#include "include/transaction.hh"
//#include "include/tuple.hh"
#include "include/util.hh"

#include "../include/cache_line_size.hh"
#include "../include/config.hh"
#include "../include/debug.hh"
//#include "../include/masstree_wrapper.hh"
#include "../include/procedure.hh"
#include "../include/random.hh"
#include "../include/tsc.hh"
#include "../include/util.hh"
#include "../include/zipf.hh"

void chkArg() {
  displayParameter();

  if (FLAGS_perc_payment > 100) {
    cout << "FLAGS_perc_payment must be 0..100 ..." << endl;
    ERR;
  }
  if (FLAGS_perc_order_status > 100) {
    cout << "FLAGS_perc_order_status must be 0..100 ..." << endl;
    ERR;
  }
  if (FLAGS_perc_delivery > 100) {
    cout << "FLAGS_perc_delivery must be 0..100 ..." << endl;
    ERR;
  }
  if (FLAGS_perc_stock_level > 100) {
    cout << "FLAGS_perc_stock_level must be 0..100 ..." << endl;
    ERR;
  }
  if (FLAGS_perc_payment + FLAGS_perc_order_status
      + FLAGS_perc_delivery + FLAGS_perc_stock_level > 100) {
    cout << "sum of FLAGS_perc_[payment,order_status,delivery,stock_level] must be 0..100 ..." << endl;
    ERR;
  }
  if (posix_memalign((void **) &ThLocalEpoch, CACHE_LINE_SIZE,
                     FLAGS_thread_num * sizeof(uint64_t_64byte)) != 0)
    ERR;
  if (posix_memalign((void **) &CTIDW, CACHE_LINE_SIZE,
                     FLAGS_thread_num * sizeof(uint64_t_64byte)) != 0)
    ERR;

  // init
  for (unsigned int i = 0; i < FLAGS_thread_num; ++i) {
    ThLocalEpoch[i].obj_ = 0;
    CTIDW[i].obj_ = 0;
  }
}

void displayParameter() {
  cout << "#FLAGS_thread_num:\t" << FLAGS_thread_num << endl;
  cout << "#FLAGS_clocks_per_us:\t" << FLAGS_clocks_per_us << endl;
  cout << "#FLAGS_epoch_time:\t" << FLAGS_epoch_time << endl;
  cout << "#FLAGS_extime:\t\t" << FLAGS_extime << endl;
  cout << "#FLAGS_max_items:\t" << FLAGS_max_items << endl;
  cout << "#FLAGS_cust_per_dist:\t" << FLAGS_cust_per_dist << endl;
  cout << "#FLAGS_perc_payment:\t\t" << FLAGS_perc_payment << endl;
  cout << "#FLAGS_perc_order_status:\t" << FLAGS_perc_order_status << endl;
  cout << "#FLAGS_perc_delivery:\t\t" << FLAGS_perc_delivery << endl;
  cout << "#FLAGS_perc_stock_level:\t" << FLAGS_perc_stock_level << endl;
}

void genLogFile([[maybe_unused]] std::string &logpath, [[maybe_unused]]const int thid) {
  /*
  genLogFileName(logpath, thid);
  createEmptyFile(logpath);
  */
}

