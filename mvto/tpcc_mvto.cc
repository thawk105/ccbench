#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <functional>
#include <thread>

#define GLOBAL_VALUE_DEFINE

#include "include/common.hh"
#include "include/result.hh"
#include "include/transaction.hh"
#include "include/util.hh"

#include "../include/atomic_wrapper.hh"
#include "../include/backoff.hh"
#include "../include/compiler.hh"
#include "../include/cpu.hh"
#include "../include/debug.hh"
#include "../include/delay.hh"
#include "../include/int64byte.hh"
#include "../include/procedure.hh"
#include "../include/random.hh"
#include "../include/result.hh"
#include "../include/util.hh"
#include "../include/zipf.hh"
#include "../include/tpcc.hh"

using namespace std;

void worker(size_t thid, char &ready, const bool &start, const bool &quit) {
  Backoff backoff(FLAGS_clocks_per_us);
  TxExecutor trans(thid, backoff, (Result *) &MvtoResult[thid], quit);
  Result &myres = std::ref(MvtoResult[thid]);
  TPCCWorkload<Tuple,TupleInitParam> workload;
  workload.prepare(trans, new TupleInitParam());

#ifdef Linux
  setThreadAffinity(thid);
#endif

#if MASSTREE_USE
  MasstreeWrapper<Tuple>::thread_init(thid);
#endif

  storeRelease(ready, 1);
  while (!loadAcquire(start)) _mm_pause();
  while (!loadAcquire(quit)) {
    workload.run<TxExecutor,TransactionStatus>(trans);
  }

  return;
}

int main(int argc, char *argv[]) try {
  gflags::SetUsageMessage("TPC-C MVTO benchmark.");
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  chkArg();
  TPCCWorkload<Tuple,TupleInitParam>::displayWorkloadParameter();
  TupleInitParam* param = new TupleInitParam();
  TPCCWorkload<Tuple,TupleInitParam>::makeDB(param);
  MinWts.store(param->initial_wts + 2, memory_order_release);

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
    MvtoResult[0].addLocalAllResult(MvtoResult[i]);
    MvtoResult[0].addLocalPerTxResult(MvtoResult[i], TxTypes);
  }
  ShowOptParameters();
  std::cout << "actual_extime:\t" << actual_extime << std::endl;
  MvtoResult[0].displayAllResult(FLAGS_clocks_per_us, FLAGS_extime, TotalThreadNum);
  std::cout << "Details per transaction type:" << std::endl;
  MvtoResult[0].displayPerTxResult(TxTypes);

  return 0;
} catch (bad_alloc) {
  ERR;
}
