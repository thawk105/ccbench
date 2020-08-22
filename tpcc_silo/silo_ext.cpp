#include <cctype>
#include <pthread.h>
#include <sched.h>
#include <cstdlib>
#include <cstring>
#include <sys/syscall.h>
#include <ctime>
#include <sys/types.h>
#include <unistd.h>
#include <algorithm>
#include <cctype>

#include "cpu.h"

#include "boost/filesystem.hpp"

#define GLOBAL_VALUE_DEFINE

#include "include/common.hh"
#include "include/result.hh"
#include "include/util.hh"

#include "../include/random.hh"
#include "../include/result.hh"
#include "../include/zipf.hh"
#include "../include/util.hh"

#include "tpcc_initializer.hpp"
#include "tpcc_query.hpp"
#include "tpcc_txn.hpp"

using namespace std;

void worker(size_t thid, char &ready, const bool &start, const bool &quit) try {

#ifdef CCBENCH_LINUX
  ccbench::setThreadAffinity(thid);
#endif

  Result &myres = std::ref(SiloResult[thid]);
  Xoroshiro128Plus rnd{};
  rnd.init();

  TPCC::Query query;
  Token token{};
  enter(token);
  TPCC::query::Option query_opt;

  const uint16_t w_id = (thid % FLAGS_num_wh) + 1;
#if 1
  // Load per warehouse if necessary.
  // thid in [0, num_th - 1].
  // w_id in [1, FLAGS_num_wh].
  // The worker thread of thid in [0, FLAGS_num_wh - 1]
  // should load data for the warehouse with w_id = thid + 1.
  if (thid < FLAGS_num_wh) {
      //::printf("load for warehouse %u ...\n", w_id);
      TPCC::Initializer::load_per_warehouse(w_id);
      //::printf("load for warehouse %u done.\n", w_id);
  }
#endif

  TPCC::HistoryKeyGenerator hkg{};
  hkg.init(thid, true);

  storeRelease(ready, 1);
  while (!loadAcquire(start)) _mm_pause();
  while (!loadAcquire(quit)) {

    query.generate(w_id, rnd, query_opt, myres);

    // TODO : add backoff work.

    if (loadAcquire(quit)) break;

    bool validation = true;

    switch (query.type) {
      case TPCC::Q_NEW_ORDER :
        validation = TPCC::run_new_order(&query.new_order, token);
        break;
      case TPCC::Q_PAYMENT :
        validation = TPCC::run_payment(&query.payment, &hkg, token);
        break;
      case TPCC::Q_ORDER_STATUS:
        //validation = TPCC::run_order_status(query.order_status);
        break;
      case TPCC::Q_DELIVERY:
        //validation = TPCC::run_delivery(query.delivery);
        break;
      case TPCC::Q_STOCK_LEVEL:
        //validation = TPCC::run_stock_level(query.stock_level);
        break;
      case TPCC::Q_NONE:
        break;
      [[maybe_unused]] defalut:
        std::abort();
    }

    if (validation) {
      ++myres.local_commit_counts_;
    } else {
      //trans.abort();
      ++myres.local_abort_counts_;
    }
  }
  leave(token);
} catch (std::exception& e) {
    std::cout << "worker thread caught error " << e.what();
    std::abort();
}

int main(int argc, char *argv[]) try {
  gflags::SetUsageMessage("Silo benchmark.");
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  chkArg();
  init();
#if 0
  TPCC::Initializer::load();
#else
  TPCC::Initializer::load_item();
  // The remaining load will be processed by each worker threads.
#endif

  alignas(CACHE_LINE_SIZE) bool start = false;
  alignas(CACHE_LINE_SIZE) bool quit = false;
  initResult();
  std::vector<char> readys(FLAGS_thread_num);
  std::vector<std::thread> thv;
  for (size_t i = 0; i < FLAGS_thread_num; ++i)
    thv.emplace_back(worker, i, std::ref(readys[i]), std::ref(start),
                     std::ref(quit));
  waitForReady(readys);
  ::printf("starting workload...\n");
  storeRelease(start, true);
  for (size_t i = 0; i < FLAGS_extime; ++i) {
    sleepMs(1000);
  }
  storeRelease(quit, true);
  for (auto &th : thv) th.join();

  for (unsigned int i = 0; i < FLAGS_thread_num; ++i) {
    SiloResult[0].addLocalAllResult(SiloResult[i]);
  }
  SiloResult[0].displayAllResult(FLAGS_clocks_per_us, FLAGS_extime,
                                 FLAGS_thread_num);

  fin();
  return 0;
} catch (std::bad_alloc &) {
  std::cout << __FILE__ << " : " << __LINE__ << " : bad_alloc error." << std::endl;
  std::abort();
} catch (std::exception& e) {
  std::cout << __FILE__ << " : " << __LINE__ << " : std::exception caught : " << e.what() << std::endl;
}
