#define GLOBAL_VALUE_DEFINE

#include "include/common.hh"
#include "include/result.hh"
#include "include/transaction.hh"
#include "include/util.hh"

#include "../../include/cpu.hh"
#include "../../include/debug.hh"
#include "../../include/int64byte.hh"
#include "../../include/random.hh"
#include "../../include/result.hh"
#include "../../include/tsc.hh"
#include "../../include/util.hh"
#include "../../include/ycsb.hh"
#include "../../include/zipf.hh"

#include "../../common/runner.hh"

#include "glog/logging.h"

using namespace std;

int main(int argc, char* argv[]) try {
  google::InitGoogleLogging(argv[0]);
  google::InstallFailureSignalHandler();

  gflags::SetUsageMessage("Oze YCSB benchmark.");
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  init(YcsbWorkload::getTableNum());
  YcsbWorkload::displayWorkloadParameter();
  YcsbWorkload::makeDB<Tuple, void>(nullptr);

  initResult(TotalThreadNum);

  ccbench::run<TxExecutor, TransactionStatus, YcsbWorkload>(
      TotalThreadNum, ccbench::RunnerOptions{},
      [](size_t thid, const bool& quit, Backoff& backoff) {
        return TxExecutor(thid, backoff, &CCBenchResults[thid], quit);
      },
      []([[maybe_unused]] TxExecutor& trans, [[maybe_unused]] size_t thid) {
#ifdef Linux
        setThreadAffinity(thid);
#endif
      },
      [](TxExecutor& trans, size_t thid) {
        if (thid == 0) trans.epoch_timer_start_ = rdtscp();
      });

  return 0;
} catch (const bad_alloc&) { ERR; }
