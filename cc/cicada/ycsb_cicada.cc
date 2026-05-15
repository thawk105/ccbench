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
#include "../../include/random.hh"
#include "../../include/result.hh"
#include "../../include/util.hh"
#include "../../include/ycsb.hh"
#include "../../include/zipf.hh"

#include "../../common/runner.hh"

using namespace std;

int main(int argc, char* argv[]) try {
  gflags::SetUsageMessage("YCSB Cicada benchmark.");
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  chkArg();
  YcsbWorkload::displayWorkloadParameter();
  TupleInitParam* param = new TupleInitParam();
  YcsbWorkload::makeDB<Tuple, TupleInitParam>(param);
  MinWts.store(param->initial_wts + 2, memory_order_release);

  initResult(TotalThreadNum);

  ccbench::RunnerOptions opts;
  opts.show_actual_extime = false; // cicada historically omitted it.
  opts.pass_op_num = true;
  opts.max_ope = FLAGS_max_ope;
  opts.batch_max_ope = FLAGS_batch_max_ope;

  ccbench::run<TxExecutor, TransactionStatus, YcsbWorkload>(
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
      });

  return 0;
} catch (const bad_alloc&) { ERR; }
