#define GLOBAL_VALUE_DEFINE

#include "include/common.hh"
#include "include/result.hh"
#include "include/transaction.hh"
#include "include/util.hh"

#include "../../include/compiler.hh"
#include "../../include/cpu.hh"
#include "../../include/debug.hh"
#include "../../include/delay.hh"
#include "../../include/int64byte.hh"
#include "../../include/masstree_wrapper.hh"
#include "../../include/random.hh"
#include "../../include/result.hh"
#include "../../include/tpcc.hh"
#include "../../include/util.hh"
#include "../../include/zipf.hh"

#include "../../common/runner.hh"

using namespace std;

int main(int argc, char* argv[]) try {
  gflags::SetUsageMessage("TPC-C Cicada benchmark.");
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  chkArg();
  TPCCWorkload<Tuple, TupleInitParam>::displayWorkloadParameter();
  TupleInitParam* param = new TupleInitParam();
  TPCCWorkload<Tuple, TupleInitParam>::makeDB(param);
  MinWts.store(param->initial_wts + 2, memory_order_release);

  initResult(TotalThreadNum);

  ccbench::RunnerOptions opts;
  opts.show_actual_extime = false;
  opts.display_per_tx = true;

  ccbench::run<TxExecutor, TransactionStatus,
               TPCCWorkload<Tuple, TupleInitParam>>(
      TotalThreadNum, opts,
      [](size_t thid, const bool& quit, Backoff& backoff) {
        return TxExecutor(thid, backoff, &CCBenchResults[thid], quit);
      },
      [](TxExecutor& /*trans*/, size_t thid) {
#ifdef Linux
        setThreadAffinity(thid);
#endif
#if MASSTREE_USE
        MasstreeWrapper<Tuple>::thread_init(thid);
#endif
      });

  return 0;
} catch (const bad_alloc&) { ERR; }
