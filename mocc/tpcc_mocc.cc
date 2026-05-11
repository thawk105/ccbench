#include <ctype.h>  // isdigit,
#include <pthread.h>
#include <string.h>       // strlen,
#include <sys/syscall.h>  // syscall(SYS_gettid),
#include <sys/types.h>    // syscall(SYS_gettid),
#include <time.h>
#include <unistd.h>  // syscall(SYS_gettid),
#include <iostream>
#include <string>  // string

#define GLOBAL_VALUE_DEFINE

#include "include/atomic_tool.hh"
#include "include/common.hh"
#include "include/result.hh"
#include "include/transaction.hh"
#include "include/util.hh"

#include "../include/atomic_wrapper.hh"
#include "../include/backoff.hh"
#include "../include/cpu.hh"
#include "../include/debug.hh"
#include "../include/int64byte.hh"
#include "../include/masstree_wrapper.hh"
#include "../include/result.hh"
#include "../include/tpcc.hh"
#include "../include/tsc.hh"
#include "../include/util.hh"
#include "../include/zipf.hh"

using namespace std;

void worker(size_t thid, char &ready, const bool &start, const bool &quit) {
  Result &myres = std::ref(MoccResult[thid]);
  TxExecutor trans(thid, &myres, quit);
  TPCCWorkload<Tuple,void> workload;
  workload.prepare(trans, nullptr);

#if MASSTREE_USE
  MasstreeWrapper<Tuple>::thread_init(int(thid));
#endif

#ifdef Linux
  setThreadAffinity(thid);
#endif  // Linux

  storeRelease(ready, 1);
  while (!loadAcquire(start)) _mm_pause();
  if (thid == 0) trans.epoch_timer_start = rdtscp();
  while (!loadAcquire(quit)) {
    workload.run<TxExecutor,TransactionStatus>(trans);
  }

  return;
}

int main(int argc, char *argv[]) try {
  gflags::SetUsageMessage("TPC-C MOCC benchmark.");
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  chkArg();
  TPCCWorkload<Tuple,void>::displayWorkloadParameter();
  TPCCWorkload<Tuple,void>::makeDB(nullptr);

  alignas(CACHE_LINE_SIZE) bool start = false;
  alignas(CACHE_LINE_SIZE) bool quit = false;
  initResult();
  std::vector<char> readys(TotalThreadNum);
  std::vector<std::thread> thv;
  for (size_t i = 0; i < TotalThreadNum; ++i)
    thv.emplace_back(worker, i, std::ref(readys[i]), std::ref(start),
                     std::ref(quit));
  waitForReady(readys);
  uint64_t start_tsc = rdtscp();
  storeRelease(start, true);
  for (size_t i = 0; i < FLAGS_extime; ++i) {
    sleepMs(1000);
  }
  storeRelease(quit, true);
  for (auto &th : thv) th.join();
  uint64_t end_tsc = rdtscp();
  long double actual_extime = round(
    (end_tsc-start_tsc) /
    ((long double)FLAGS_clocks_per_us * powl(10.0, 6.0)));

  for (unsigned int i = 0; i < TotalThreadNum; ++i) {
    MoccResult[0].addLocalAllResult(MoccResult[i]);
    MoccResult[0].addLocalPerTxResult(MoccResult[i], TxTypes);
  }
  ShowOptParameters();
  std::cout << "actual_extime:\t" << actual_extime << std::endl;
  MoccResult[0].displayAllResult(FLAGS_clocks_per_us, FLAGS_extime, TotalThreadNum);
  std::cout << "Details per transaction type:" << std::endl;
  MoccResult[0].displayPerTxResult(TxTypes);

  return 0;
} catch (bad_alloc) {
  ERR;
}
