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
#include "../../include/tsc.hh"
#include "../../include/util.hh"
#include "../../include/ycsb.hh"

#include "../../common/runner.hh"

using namespace std;

int main(int argc, char* argv[]) try {
  gflags::SetUsageMessage("YCSB Silo benchmark.");
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  chkArg();
  YcsbWorkload::displayWorkloadParameter();
  YcsbWorkload::makeDB<Tuple, void>(nullptr);

  initResult(TotalThreadNum);

  ccbench::run<TxExecutor, TransactionStatus, YcsbWorkload>(
      TotalThreadNum, ccbench::RunnerOptions{},
      // make_tx: silo-style, backoff is built into the executor when
      // BACK_OFF is defined; the runner-supplied `Backoff&` is unused.
      [](size_t thid, const bool& quit, Backoff& /*unused*/) {
        return TxExecutor(thid, &CCBenchResults[thid], quit);
      },
      // worker_init: WAL log file open (when WAL is defined),
      // setThreadAffinity, MasstreeWrapper thread_init.
      [](TxExecutor& trans, size_t thid) {
#if WAL
        std::string logpath;
        genLogFile(logpath, thid);
        trans.logfile_.open(logpath, O_CREAT | O_TRUNC | O_WRONLY, 0644);
        trans.logfile_.ftruncate(10 ^ 9);
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
      // worker_after_start: leader thread records epoch start TSC.
      [](TxExecutor& trans, size_t thid) {
        if (thid == 0) trans.epoch_timer_start = rdtscp();
      });

  return 0;
} catch (const bad_alloc&) { ERR; }
