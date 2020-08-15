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

void worker(size_t thid, char &ready, const bool &start, const bool &quit) {
  Result &myres = std::ref(SiloResult[thid]);
  Xoroshiro128Plus rnd{};
  rnd.init();

  TPCC::Query query;
  TPCC::query::Option query_opt;
  query_opt.perc_payment = 100;

#ifdef CCBENCH_LINUX
  ccbench::setThreadAffinity(thid);
#endif

  storeRelease(ready, 1);
  while (!loadAcquire(start)) _mm_pause();
  while (!loadAcquire(quit)) {
    query.generate(rnd, query_opt, myres);

    // TODO : add backoff work.

    if (loadAcquire(quit)) break;

    bool validation = true;

    switch (query.type) {
      case TPCC::Q_NEW_ORDER :
        validation = TPCC::run_new_order(&query.new_order);
        break;
      case TPCC::Q_PAYMENT :
        validation = TPCC::run_payment(&query.payment);
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
      storeRelease(myres.local_commit_counts_,
                   loadAcquire(myres.local_commit_counts_) + 1);
    } else {
      //trans.abort();
      ++myres.local_abort_counts_;
    }
  }
}

int main(int argc, char *argv[]) try {
  gflags::SetUsageMessage("Silo benchmark.");
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  chkArg();
  TPCC::Initializer::load(FLAGS_num_wh);

  alignas(CACHE_LINE_SIZE) bool start = false;
  alignas(CACHE_LINE_SIZE) bool quit = false;
  initResult();
  std::vector<char> readys(FLAGS_thread_num);
  std::vector<std::thread> thv;
  for (size_t i = 0; i < FLAGS_thread_num; ++i)
    thv.emplace_back(worker, i, std::ref(readys[i]), std::ref(start),
                     std::ref(quit));
  waitForReady(readys);
  storeRelease(start, true);
  for (size_t i = 0; i < FLAGS_extime; ++i) {
    sleepMs(1000);
  }
  storeRelease(quit, true);
  for (auto &th : thv) th.join();

  for (unsigned int i = 0; i < FLAGS_thread_num; ++i) {
    SiloResult[0].addLocalAllResult(SiloResult[i]);
  }
  ShowOptParameters();
  SiloResult[0].displayAllResult(FLAGS_clocks_per_us, FLAGS_extime,
                                 FLAGS_thread_num);

  return 0;
} catch (std::bad_alloc &) {
  std::cout << __FILE__ << " : " << __LINE__ << " : fatal error." << std::endl;
  std::abort();
}
