
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

#include "../include/config.hh"
#include "../include/debug.hh"
#include "../include/inline.hh"
#include "../include/random.hh"
#include "../include/result.hh"
#include "../include/zipf.hh"
#include "include/common.hh"
#include "include/transaction.hh"
#include "include/tuple.hh"
#include "include/util.hh"

using namespace std;

void chkArg() {
  displayParameter();

  if (FLAGS_rratio > 100) {
    cout << "rratio must be 0 ~ 10" << endl;
    ERR;
  }

  if (FLAGS_zipf_skew >= 1) {
    cout << "FLAGS_zipf_skew must be 0 ~ 0.999..." << endl;
    ERR;
  }

  return;
}

void displayDB() {
  Tuple *tuple;

  for (unsigned int i = 0; i < FLAGS_tuple_num; ++i) {
    tuple = &Table[i];
    cout << "------------------------------" << endl;  //-は30個
    cout << "key: " << i << endl;
    cout << "val_: " << tuple->val_ << endl;
    cout << "TS_word: " << tuple->tsw_.obj_ << endl;
    cout << "bit: " << static_cast<bitset<64>>(tuple->tsw_.obj_) << endl;
    cout << endl;
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

void ShowOptParameters() {
  cout << "#ShowOptParameters()"
       << ": ADD_ANALYSIS " << ADD_ANALYSIS << ": BACK_OFF " << BACK_OFF
       << ": KEY_SIZE " << KEY_SIZE << ": MASSTREE_USE " << MASSTREE_USE
       << ": NO_WAIT_OF_TICTOC " << NO_WAIT_OF_TICTOC
       << ": NO_WAIT_LOCKING_IN_VALIDATION " << NO_WAIT_LOCKING_IN_VALIDATION
       << ": PREEMPTIVE_ABORTS " << PREEMPTIVE_ABORTS << ": SLEEP_READ_PHASE "
       << SLEEP_READ_PHASE << ": TIMESTAMP_HISTORY " << TIMESTAMP_HISTORY
       << ": VAL_SIZE " << VAL_SIZE << endl;
}

void makeDB() {
  if (posix_memalign((void **) &Table, PAGE_SIZE,
                     (FLAGS_tuple_num) * sizeof(Tuple)) != 0)
    ERR;
#if dbs11
  if (madvise((void *)Table, (FLAGS_tuple_num) * sizeof(Tuple),
              MADV_HUGEPAGE) != 0)
    ERR;
#endif

  size_t maxthread = decideParallelBuildNumber(FLAGS_tuple_num);

  std::vector<std::thread> thv;
  for (size_t i = 0; i < maxthread; ++i)
    thv.emplace_back(partTableInit, i, i * (FLAGS_tuple_num / maxthread),
                     (i + 1) * (FLAGS_tuple_num / maxthread) - 1);
  for (auto &th : thv) th.join();
}
