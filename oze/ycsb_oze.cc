#include <ctype.h>
#include <pthread.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <functional>
#include <random>
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
#include "../include/ycsb.hh"
#include "../include/zipf.hh"

using namespace std;

void worker(size_t thid, char &ready, const bool &start, const bool &quit) {
  YcsbWorkload workload;
  Backoff backoff(FLAGS_clocks_per_us);
  TxExecutor trans(thid, backoff, (Result *) &OzeResult[thid], quit);
  Result &myres = std::ref(OzeResult[thid]);
  uint64_t epoch_timer_start, epoch_timer_stop;

#ifdef Linux
  setThreadAffinity(thid);
  // printf("Thread #%d: on CPU %d\n", *myid, sched_getcpu());
  // printf("sysconf(_SC_NPROCESSORS_CONF) %d\n",
  // sysconf(_SC_NPROCESSORS_CONF));
#endif  // Linux

#ifdef Darwin
  int nowcpu;
  GETCPU(nowcpu);
  // printf("Thread %d on CPU %d\n", *myid, nowcpu);
#endif  // Darwin

  storeRelease(ready, 1);
  while (!loadAcquire(start)) _mm_pause();
  if (thid == 0) trans.epoch_timer_start_ = rdtscp();
  while (!loadAcquire(quit)) {
    workload.run<TxExecutor,TransactionStatus>(trans);
  }

  return;
}

int main(int argc, char *argv[]) try {
  google::InitGoogleLogging(argv[0]);
  google::InstallFailureSignalHandler();

  gflags::SetUsageMessage("Oze benchmark.");
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  init(YcsbWorkload::getTableNum());
  YcsbWorkload::displayWorkloadParameter();
  YcsbWorkload::makeDB<Tuple,void>(nullptr);

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
    OzeResult[0].addLocalAllResult(OzeResult[i]);
  }
  ShowOptParameters();
  OzeResult[0].displayAllResult(FLAGS_clocks_per_us, FLAGS_extime, TotalThreadNum);

  return 0;
} catch (bad_alloc) {
  ERR;
}
