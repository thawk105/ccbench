#define GLOBAL_VALUE_DEFINE

#include "include/common.hh"
#include "include/result.hh"
#include "include/transaction.hh"
#include "include/util.hh"

#include "../../include/cpu.hh"
#include "../../include/debug.hh"
#include "../../include/masstree_wrapper.hh"
#include "../../include/result.hh"
#include "../../include/tsc.hh"
#include "../../include/util.hh"
#include "../../include/ycsb.hh"

#include "../../common/runner.hh"

int main(int argc, char* argv[]) try {
  gflags::SetUsageMessage("YCSB SS2PL benchmark.");
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  chkArg();
  YcsbWorkload::displayWorkloadParameter();
  YcsbWorkload::makeDB<Tuple, void>(nullptr);

  initResult(TotalThreadNum);

  ccbench::run<TxExecutor, TransactionStatus, YcsbWorkload>(
      TotalThreadNum, ccbench::RunnerOptions{},
      [](std::size_t thid, const bool& quit, Backoff& /*unused*/) {
        return TxExecutor(thid, &CCBenchResults[thid], quit);
      },
      [](TxExecutor& /*trans*/, std::size_t thid) {
#ifdef Linux
        setThreadAffinity(thid);
#endif
#if MASSTREE_USE
        MasstreeWrapper<Tuple>::thread_init(static_cast<int>(thid));
#endif
      });

  return 0;
} catch (const std::bad_alloc&) { ERR; }
