#include <fcntl.h>

#define GLOBAL_VALUE_DEFINE

#include "include/atomic_tool.hh"
#include "include/common.hh"
#include "include/result.hh"
#include "include/transaction.hh"
#include "include/util.hh"

#include "../../include/cpu.hh"
#include "../../include/debug.hh"
#include "../../include/fileio.hh"
#include "../../include/masstree_wrapper.hh"
#include "../../include/result.hh"
#include "../../include/tpcc.hh"
#include "../../include/tsc.hh"
#include "../../include/util.hh"

#include "../../common/runner.hh"

using namespace std;

int main(int argc, char* argv[]) try {
  gflags::SetUsageMessage("TPC-C Silo benchmark.");
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  chkArg();
  TPCCWorkload<Tuple, void>::displayWorkloadParameter();
  TPCCWorkload<Tuple, void>::makeDB(nullptr);

  initResult(TotalThreadNum);

  ccbench::RunnerOptions opts;
  opts.display_per_tx = true;

  ccbench::run<TxExecutor, TransactionStatus, TPCCWorkload<Tuple, void>>(
      TotalThreadNum, opts,
      [](size_t thid, const bool& quit, Backoff& /*unused*/) {
        return TxExecutor(thid, &CCBenchResults[thid], quit);
      },
      [](TxExecutor& trans, size_t thid) {
#if WAL
        std::string logpath;
        genLogFile(logpath, thid);
        trans.logfile_.open(logpath, O_CREAT | O_TRUNC | O_WRONLY, 0644);
        trans.logfile_.ftruncate(1000000000);
#else
        (void) trans;
#endif
#ifdef Linux
        setThreadAffinity(thid);
#endif
#if MASSTREE_USE
        MasstreeWrapper<Tuple>::thread_init(int(thid));
#endif
      },
      [](TxExecutor& trans, size_t thid) {
        if (thid == 0) trans.epoch_timer_start = rdtscp();
      });

  return 0;
} catch (const bad_alloc&) { ERR; }
