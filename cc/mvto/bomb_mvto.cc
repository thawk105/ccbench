#define GLOBAL_VALUE_DEFINE

#include "include/common.hh"
#include "include/result.hh"
#include "include/transaction.hh"
#include "include/util.hh"

#include "../../include/bomb.hh"
#include "../../include/cpu.hh"
#include "../../include/debug.hh"
#include "../../include/int64byte.hh"
#include "../../include/masstree_wrapper.hh"
#include "../../include/random.hh"
#include "../../include/result.hh"
#include "../../include/tsc.hh"
#include "../../include/util.hh"
#include "../../include/zipf.hh"

#include "../../common/runner.hh"

using namespace std;

int main(int argc, char* argv[]) try {
  gflags::SetUsageMessage("BOMB MVTO benchmark.");
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  chkArg();
  BombWorkload<Tuple, TupleInitParam>::displayWorkloadParameter();
  TupleInitParam* param = new TupleInitParam();
  BombWorkload<Tuple, TupleInitParam>::makeDB(param);
  MinWts.store(param->initial_wts + 2, memory_order_release);

  initResult(TotalThreadNum);

  ccbench::RunnerOptions opts;
  opts.display_per_tx = true;
  opts.enable_bomb_dispatcher = FLAGS_bomb_mixed_mode;

  ccbench::run<TxExecutor, TransactionStatus,
               BombWorkload<Tuple, TupleInitParam>>(
      TotalThreadNum, opts,
      [](size_t thid, const bool& quit, Backoff& backoff) {
        return TxExecutor(thid, backoff, &CCBenchResults[thid], quit);
      },
      [](TxExecutor& /*trans*/, size_t thid) {
#if MASSTREE_USE
        MasstreeWrapper<Tuple>::thread_init(int(thid));
#endif
#ifdef Linux
        setThreadAffinity(thid);
#endif
      },
      ccbench::NoOpHook{},
      &BombWorkload<Tuple, TupleInitParam>::request_dispatcher);

  return 0;
} catch (const bad_alloc&) { ERR; }
