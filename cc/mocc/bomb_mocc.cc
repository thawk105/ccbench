#define GLOBAL_VALUE_DEFINE

#include "include/atomic_tool.hh"
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

using namespace std;

int main(int argc, char* argv[]) try {
  gflags::SetUsageMessage("BOMB MOCC benchmark.");
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  chkArg();
  BombWorkload<Tuple, void>::displayWorkloadParameter();
  BombWorkload<Tuple, void>::makeDB(nullptr);

  initResult(TotalThreadNum);

  ccbench::RunnerOptions opts;
  opts.display_per_tx = true;
  opts.enable_bomb_dispatcher = FLAGS_bomb_mixed_mode;

  ccbench::run<TxExecutor, TransactionStatus, BombWorkload<Tuple, void>>(
      TotalThreadNum, opts,
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
      [](TxExecutor& trans, size_t thid) {
        if (thid == 0) trans.epoch_timer_start = rdtscp();
      },
      &BombWorkload<Tuple, void>::request_dispatcher);

  return 0;
} catch (const bad_alloc&) { ERR; }
