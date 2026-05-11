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

#include "include/atomic_tool.hh"
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
#include "../include/bomb.hh"
#include "../include/zipf.hh"

using namespace std;

void worker(size_t thid, char &ready, const bool &start, const bool &quit) {
  Result &myres = std::ref(SiloResult[thid]);
  TxExecutor trans(thid, (Result *) &myres, quit);
  BombWorkload<Tuple,void> workload;
  workload.prepare(trans, nullptr);
#if BACK_OFF
  Backoff backoff(FLAGS_clocks_per_us);
#endif

#if WAL
  /*
  const boost::filesystem::path log_dir_path("/tmp/ccbench");
  if (boost::filesystem::exists(log_dir_path)) {
  } else {
    boost::system::error_code error;
    const bool result = boost::filesystem::create_directory(log_dir_path,
  error); if (!result || error) { ERR;
    }
  }
  std::string logpath("/tmp/ccbench");
  */
  std::string logpath;
  genLogFile(logpath, thid);
  trans.logfile_.open(logpath, O_CREAT | O_TRUNC | O_WRONLY, 0644);
  trans.logfile_.ftruncate(10 ^ 9);
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
  if (thid == 0) trans.epoch_timer_start = rdtscp();
  while (!loadAcquire(quit)) {
    workload.run<TxExecutor,TransactionStatus>(trans);
  }

  return;
}

int main(int argc, char *argv[]) try {
  gflags::SetUsageMessage("BOMB Silo benchmark.");
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  chkArg();
  BombWorkload<Tuple,void>::displayWorkloadParameter();
  BombWorkload<Tuple,void>::makeDB(nullptr);

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
    SiloResult[0].addLocalAllResult(SiloResult[i]);
    SiloResult[0].addLocalPerTxResult(SiloResult[i], TxTypes);
  }
  ShowOptParameters();
  std::cout << "actual_extime:\t" << actual_extime << std::endl;
  SiloResult[0].displayAllResult(FLAGS_clocks_per_us, FLAGS_extime, TotalThreadNum);
  std::cout << "Details per transaction type:" << std::endl;
  SiloResult[0].displayPerTxResult(TxTypes);

  return 0;
} catch (bad_alloc) {
  ERR;
}
