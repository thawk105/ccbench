#define GLOBAL_VALUE_DEFINE

#include "include/common.hh"
#include "include/garbage_collection.hh"
#include "include/result.hh"
#include "include/transaction.hh"
#include "include/util.hh"

#include "../../include/cpu.hh"
#include "../../include/debug.hh"
#include "../../include/int64byte.hh"
#include "../../include/masstree_wrapper.hh"
#include "../../include/random.hh"
#include "../../include/result.hh"
#include "../../include/tpcc.hh"
#include "../../include/tsc.hh"
#include "../../include/util.hh"
#include "../../include/zipf.hh"

#include "../../common/runner.hh"

using namespace std;

int main(int argc, char* argv[]) try {
  gflags::SetUsageMessage("TPC-C SI benchmark.");
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  chkArg();
  TPCCWorkload<Tuple, void>::displayWorkloadParameter();
  TPCCWorkload<Tuple, void>::makeDB(nullptr);

  initResult(TotalThreadNum);

  ccbench::RunnerOptions opts;
  opts.display_per_tx = true;

  ccbench::run<TxExecutor, TransactionStatus, TPCCWorkload<Tuple, void>>(
      TotalThreadNum, opts,
      [](size_t thid, const bool& quit, Backoff& backoff) {
        return TxExecutor(thid, backoff, &CCBenchResults[thid], quit);
      },
      [](TxExecutor& trans, size_t thid) {
#if MASSTREE_USE
        MasstreeWrapper<Tuple>::thread_init(int(thid));
#endif
#ifdef Linux
        setThreadAffinity(thid);
#endif
        if (trans.isLeader()) trans.gcob.decideFirstRange();
        trans.gcstart_ = rdtscp();
      });

  return 0;
} catch (const bad_alloc&) { ERR; }
