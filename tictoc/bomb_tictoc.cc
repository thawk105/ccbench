
#include <ctype.h>
#include <pthread.h>
#include <sched.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <x86intrin.h>

#include <algorithm>
#include <cctype>

#define GLOBAL_VALUE_DEFINE

#include "include/common.hh"
#include "include/result.hh"
#include "include/transaction.hh"
#include "include/util.hh"

#include "../include/atomic_wrapper.hh"
#include "../include/backoff.hh"
#include "../include/bomb.hh"
#include "../include/cpu.hh"
#include "../include/debug.hh"
#include "../include/masstree_wrapper.hh"
#include "../include/random.hh"
#include "../include/result.hh"
#include "../include/tsc.hh"
#include "../include/util.hh"
#include "../include/zipf.hh"

void worker(size_t thid, char &ready, const bool &start, const bool &quit) {
  Result &myres = std::ref(TicTocResult[thid]);
  TxExecutor trans(thid, (Result *) &myres, quit);
  BombWorkload<Tuple,void> workload;
  workload.prepare(trans, nullptr);

#if BACK_OFF
  Backoff backoff(FLAGS_clocks_per_us);
#endif

#if MASSTREE_USE
  MasstreeWrapper<Tuple>::thread_init(int(thid));
#endif

#ifdef Linux
  setThreadAffinity(thid);
  // size_t cpu_id = thid / 4 + thid % 4 * 28;
  // setThreadAffinity(cpu_id);
  // printf("Thread %zu, affi %zu\n", thid, cpu_id);
  // printf("Thread #%d: on CPU %d\n", *myid, sched_getcpu());
  // printf("sysconf(_SC_NPROCESSORS_CONF) %d\n",
  // sysconf(_SC_NPROCESSORS_CONF));
#endif

  storeRelease(ready, 1);
  while (!loadAcquire(start)) _mm_pause();
  while (!loadAcquire(quit)) {
    workload.run<TxExecutor,TransactionStatus>(trans);
  }

  return;
}

int main(int argc, char *argv[]) try {
  gflags::SetUsageMessage("BOMB TicToc benchmark.");
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
  storeRelease(start, true);
  for (size_t i = 0; i < FLAGS_extime; ++i) {
    sleepMs(1000);
  }
  storeRelease(quit, true);
  for (auto &th : thv) th.join();

  for (unsigned int i = 0; i < FLAGS_thread_num; ++i) {
    TicTocResult[0].addLocalAllResult(TicTocResult[i]);
    TicTocResult[0].addLocalPerTxResult(TicTocResult[i], TxTypes);
  }
  ShowOptParameters();
  TicTocResult[0].displayAllResult(FLAGS_clocks_per_us, FLAGS_extime,
                                   FLAGS_thread_num);
  std::cout << "Details per transaction type:" << std::endl;
  TicTocResult[0].displayPerTxResult(TxTypes);

  return 0;
} catch (bad_alloc) {
  ERR;
}
