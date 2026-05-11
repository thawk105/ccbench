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
#include "../include/zipf.hh"
#include "../include/bomb.hh"

using namespace std;

void worker(size_t thid, char &ready, const bool &start, const bool &quit) {
  Backoff backoff(FLAGS_clocks_per_us);
  TxExecutor trans(thid, backoff, (Result *) &CicadaResult[thid], quit);
  Result &myres = std::ref(CicadaResult[thid]);
  BombWorkload<Tuple,TupleInitParam> workload;
  workload.prepare(trans, new TupleInitParam());

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
  gflags::SetUsageMessage("BOMB Cicada benchmark.");
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  chkArg();
  BombWorkload<Tuple,TupleInitParam>::displayWorkloadParameter();
  TupleInitParam* param = new TupleInitParam();
  BombWorkload<Tuple,TupleInitParam>::makeDB(param);
  MinWts.store(param->initial_wts + 2, memory_order_release);

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

  for (unsigned int i = 0; i < TotalThreadNum; ++i) {
    CicadaResult[0].addLocalAllResult(CicadaResult[i]);
    CicadaResult[0].addLocalPerTxResult(CicadaResult[i], TxTypes);
  }
  ShowOptParameters();
  CicadaResult[0].displayAllResult(FLAGS_clocks_per_us, FLAGS_extime, TotalThreadNum);
  std::cout << "Details per transaction type:" << std::endl;
  CicadaResult[0].displayPerTxResult(TxTypes);
  // TODO: enable this if really necessary
  // deleteDB();

  return 0;
} catch (bad_alloc) {
  ERR;
}
