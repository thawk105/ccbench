#include <ctype.h>
#include <pthread.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <algorithm>
#include <cctype>

#include "boost/filesystem.hpp"

#define GLOBAL_VALUE_DEFINE

#include "include/common.hh"
#include "include/result.hh"
#include "include/transaction.hh"
#include "include/util.hh"

#include "../include/atomic_wrapper.hh"
#include "../include/backoff.hh"
#include "../include/cpu.hh"
#include "../include/debug.hh"
#include "../include/fileio.hh"
#include "../include/masstree_wrapper.hh"
#include "../include/random.hh"
#include "../include/result.hh"
#include "../include/tsc.hh"
#include "../include/util.hh"
#include "../include/dbomb_deterministic.hh"
#include "../include/zipf.hh"

using namespace std;

void worker(size_t thid, char &ready, const bool &start, const bool &quit) {
  Result &myres = std::ref(D2PLResult[thid]);
  TxExecutor trans(thid, (Result*)&myres, quit);
  DeterministicBombWorkload<Tuple,void> workload;
  workload.prepare(trans, nullptr);
#if BACK_OFF
  Backoff backoff(FLAGS_clocks_per_us);
#endif

#ifdef Linux
  setThreadAffinity(thid);
  // printf("Thread #%d: on CPU %d\n", res.thid_, sched_getcpu());
  // printf("sysconf(_SC_NPROCESSORS_CONF) %d\n",
  // sysconf(_SC_NPROCESSORS_CONF));
#endif

#if MASSTREE_USE
  MasstreeWrapper<Tuple>::thread_init(int(thid));
#endif

  storeRelease(ready, 1);
  while (!loadAcquire(start)) _mm_pause();
  while (!loadAcquire(quit)) {
    workload.run<TxExecutor,TransactionStatus>(trans);
  }

  return;
}

int main(int argc, char *argv[]) try {
  gflags::SetUsageMessage("Determinisitc BOMB D2PL benchmark.");
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  chkArg();
  DeterministicBombWorkload<Tuple,void>::displayWorkloadParameter();
  DeterministicBombWorkload<Tuple,void>::makeDB(nullptr);

  alignas(CACHE_LINE_SIZE) bool start = false;
  alignas(CACHE_LINE_SIZE) bool quit = false;
  initResult();
  std::vector<char> readys(TotalThreadNum + (FLAGS_bomb_mixed_mode ? 1 : 0));
  std::vector<std::thread> thv;
  for (size_t i = 0; i < TotalThreadNum; ++i)
    thv.emplace_back(worker, i, std::ref(readys[i]),
                     std::ref(start), std::ref(quit));
  if (FLAGS_bomb_mixed_mode) {
    thv.emplace_back(BombWorkload<Tuple,void>::request_dispatcher,
                     TotalThreadNum, std::ref(readys[TotalThreadNum]),
                     std::ref(start), std::ref(quit));
  }
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
    D2PLResult[0].addLocalAllResult(D2PLResult[i]);
    D2PLResult[0].addLocalPerTxResult(D2PLResult[i], TxTypes);
  }
  ShowOptParameters();
  std::cout << "actual_extime:\t" << actual_extime << std::endl;
  D2PLResult[0].displayAllResult(FLAGS_clocks_per_us, FLAGS_extime, TotalThreadNum);
  std::cout << "Details per transaction type:" << std::endl;
  D2PLResult[0].displayPerTxResult(TxTypes);

  return 0;
} catch (bad_alloc) {
  ERR;
}
