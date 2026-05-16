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
  gflags::SetUsageMessage("YCSB TicToc benchmark.");
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  chkArg();
  YcsbWorkload::displayWorkloadParameter();
  YcsbWorkload::makeDB<Tuple, void>(nullptr);

  // tictoc has no batch threads, so `TotalThreadNum == FLAGS_thread_num`.
  // We pass `FLAGS_thread_num` to keep the historical output identical.
  initResult(TotalThreadNum);

  ccbench::RunnerOptions opts;
  opts.show_actual_extime = false; // historical tictoc output omitted it.

  ccbench::run<TxExecutor, TransactionStatus, YcsbWorkload>(
      FLAGS_thread_num, opts,
      [](size_t thid, const bool& quit, Backoff& /*unused*/) {
        return TxExecutor(thid, &CCBenchResults[thid], quit);
      },
      [](TxExecutor& /*trans*/, size_t thid) {
#if MASSTREE_USE
        MasstreeWrapper<Tuple>::thread_init(int(thid));
#endif
#ifdef Linux
        setThreadAffinity(thid);
#endif
      });

  return 0;
} catch (const std::bad_alloc&) { ERR; }
