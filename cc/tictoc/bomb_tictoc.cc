#define GLOBAL_VALUE_DEFINE

#include "include/common.hh"
#include "include/result.hh"
#include "include/transaction.hh"
#include "include/util.hh"

#include "../../include/bomb.hh"
#include "../../include/cpu.hh"
#include "../../include/debug.hh"
#include "../../include/masstree_wrapper.hh"
#include "../../include/result.hh"
#include "../../include/tsc.hh"
#include "../../include/util.hh"

#include "../../common/runner.hh"

int main(int argc, char* argv[]) try {
  gflags::SetUsageMessage("BOMB TicToc benchmark.");
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  chkArg();
  BombWorkload<Tuple, void>::displayWorkloadParameter();
  BombWorkload<Tuple, void>::makeDB(nullptr);

  initResult(TotalThreadNum);

  ccbench::RunnerOptions opts;
  opts.show_actual_extime = false;
  opts.display_per_tx = true;
  opts.enable_bomb_dispatcher = FLAGS_bomb_mixed_mode;

  ccbench::run<TxExecutor, TransactionStatus, BombWorkload<Tuple, void>>(
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
      },
      ccbench::NoOpHook{}, &BombWorkload<Tuple, void>::request_dispatcher);

  return 0;
} catch (const std::bad_alloc&) { ERR; }
