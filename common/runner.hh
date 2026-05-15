#pragma once

/**
 * @file runner.hh
 * @brief Workload-entrypoint orchestration template shared by every protocol.
 *
 * Every protocol's `<workload>_<proto>.cc` used to repeat the same 90-120
 * lines: gflags parse, `chkArg()`, `displayWorkloadParameter()` /
 * `makeDB()`, allocate `start`/`quit` flags, spawn `TotalThreadNum`
 * workers, wait on a `ready` barrier, sleep `FLAGS_extime` seconds, signal
 * `quit`, join, then fold every thread's `Result` into slot 0 and call
 * `displayAllResult()`. Only the `TxExecutor` constructor shape (plain vs
 * `Backoff`-taking), a handful of per-protocol init hooks (Silo's WAL log
 * file, Cicada's `MinWts.store()`, Ermia/SI's `gcob.decideFirstRange()`
 * + `gcstart_`), and the `displayAllResult` overload selection actually
 * differ - the boilerplate vastly exceeds the variant payload.
 *
 * `ccbench::run<Tx, Workload>(...)` collapses the boilerplate into one
 * template. Per-protocol variants travel through:
 *   - a factory functor that constructs the `Tx` (lets Silo skip the
 *     `Backoff` arg while Cicada / Ermia / MVTO / Oze / SI pass one);
 *   - a `WorkerInit` hook called inside each worker thread after the
 *     `Tx` is constructed (MasstreeWrapper thread_init, setThreadAffinity,
 *     WAL log file open, `workload.prepare()`, `gcob.decideFirstRange()`,
 *     ...);
 *   - a `WorkerAfterStart` hook called inside each worker right after
 *     the `start` barrier (`trans.epoch_timer_start = rdtscp()`,
 *     `trans.gcstart_ = rdtscp()`);
 *   - a small `RunnerOptions` struct of bools that pick the right
 *     `displayAllResult` overload (op_num / no op_num), whether to print
 *     `actual_extime`, and whether to spawn the BoMB `request_dispatcher`
 *     thread.
 *
 * The per-thread `Result` vector is the project-wide `CCBenchResults`
 * defined in `common/result.cc` (see #101). Runner never spells a
 * per-protocol global name, so adding a new protocol or workload only
 * touches the entrypoint, not this file.
 *
 * Issue: #102. Sister issue #101 (per-protocol `result.cc` removal) is
 * already in master.
 */

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "include/atomic_wrapper.hh"
#include "include/backoff.hh"
#include "include/cache_line_size.hh"
#include "include/result.hh"
#include "include/tsc.hh"
#include "include/tx_executor_concept.hh"
#include "include/util.hh"

#include <map>
#include <string>

#include "gflags/gflags.h"

// Forward declarations of names the runner uses that travel through
// per-protocol headers normally. The caller is expected to
// `#include "common/runner.hh"` *after* its own `include/common.hh`
// (which provides the real `DECLARE_uint64(...)` etc. via gflags) and
// after whichever workload header defines `TxTypes` via the per-protocol
// `GLOBAL` macro. Declaring them here too keeps the runner header
// self-contained for compile-time lookup: every name the template body
// touches must be visible at the *definition* point because none of
// them depend on a template parameter.
//
// FLAGS_*: gflags `DECLARE_*` produces `extern <type> FLAGS_<name>;`,
// so a second `DECLARE_*` from runner.hh is harmless (just a redecl).
DECLARE_uint64(clocks_per_us);
DECLARE_uint64(extime);

// `TxTypes` is also declared in `include/workload.hh` via the `GLOBAL`
// macro - here it is a plain `extern` so the runner does not depend on
// any protocol's `GLOBAL` definition.
extern std::map<uint32_t, std::string> TxTypes;

// `ShowOptParameters()` is defined per-protocol in `cc/<proto>/util.cc`
// and declared in `cc/<proto>/include/util.hh` - the protocol's static
// library resolves the symbol at link time.
extern void ShowOptParameters();

namespace ccbench {

/**
 * @brief Display / threading variants that differ across `<wl>_<proto>`
 *        entrypoints but not enough to justify multiple runner templates.
 *
 * Defaults match the most common shape (no op_num, no per-tx breakdown,
 * prints `actual_extime`, no BoMB dispatcher). Specific binaries flip
 * the few bits they need:
 *   - cicada / ermia / si YCSB pass FLAGS_max_ope / FLAGS_batch_max_ope
 *     to `displayAllResult` (`pass_op_num`);
 *   - every TPC-C and BoMB binary prints a per-tx-type breakdown
 *     (`display_per_tx`);
 *   - every BoMB binary except `sbomb` and `dbomb` (deterministic)
 *     spawns the mixed-mode `request_dispatcher` (`bomb_dispatcher`);
 *   - cicada YCSB / TPC-C never printed `actual_extime` historically
 *     (`show_actual_extime = false`).
 */
struct RunnerOptions {
  /// Print `"actual_extime:\t<seconds>"` (measured from `rdtscp()`
  /// before/after the run loop). Disabled on cicada to preserve
  /// historical output.
  bool show_actual_extime = true;
  /// Pass `max_ope` / `batch_max_ope` (= the caller's `FLAGS_max_ope` /
  /// `FLAGS_batch_max_ope`) to `displayAllResult`. Used by cicada /
  /// ermia / si YCSB binaries that report per-operation throughput.
  /// The runner cannot reference `FLAGS_max_ope` / `FLAGS_batch_max_ope`
  /// directly because not every protocol defines them (d2pl / ss2pl /
  /// tictoc lack `batch_max_ope`; ss2pl test build lacks `max_ope`).
  bool pass_op_num = false;
  std::uint64_t max_ope = 0;
  std::uint64_t batch_max_ope = 0;
  /// Call `addLocalPerTxResult` + `displayPerTxResult` after the
  /// main `addLocalAllResult` loop. Used by TPC-C and BoMB binaries.
  bool display_per_tx = false;
  /// Spawn `BombWorkload<Tuple, void>::request_dispatcher` as an extra
  /// thread (at index `TotalThreadNum`) when `FLAGS_bomb_mixed_mode`
  /// is set. The dispatcher itself is supplied via `bomb_dispatcher_fn`
  /// so the runner header does not need to mention `BombWorkload`.
  bool enable_bomb_dispatcher = false;
};

/**
 * @brief Signature of the BoMB mixed-mode request dispatcher.
 *
 * Always `BombWorkload<Tuple, void>::request_dispatcher` in practice,
 * even for `sbomb` (static) and `dbomb` (deterministic) entrypoints that
 * historically referenced the non-Static `BombWorkload` form to share
 * the dispatcher queue. Pass it explicitly so this header does not
 * depend on `bomb.hh`.
 */
using BombDispatcherFn = void (*)(std::size_t thid, char& ready,
                                  const bool& start, const bool& quit);

/// Default no-op hook for `worker_init` / `worker_after_start`.
struct NoOpHook {
  template <typename Tx>
  void operator()(Tx&, std::size_t) const noexcept {}
};

namespace runner_detail {

template <typename W, typename Tx>
concept HasPrepare = requires(W w, Tx& trans) {
  w.prepare(trans, nullptr);
};

/// Worker-thread body. Factored out of `ccbench::run` so the heavy
/// inner loop stays a free function template the compiler can inline
/// through. Hooks travel by const reference - they live on the parent
/// stack frame and outlive the worker.
///
/// `make_tx(thid, quit, backoff)` receives the worker's thread id,
/// a `const bool&` to the runner's `quit` flag (several `TxExecutor`
/// constructors keep a reference to it for their retry loops), and a
/// `Backoff&` on the worker's stack (used by cicada / ermia / mvto /
/// oze / si, ignored by silo / mocc / ss2pl / tictoc / d2pl).
template <typename Tx, typename TransactionStatus, typename Workload,
          typename MakeTx, typename WorkerInit, typename WorkerAfterStart>
requires TxExecutorLike<Tx>
void worker_body(std::size_t thid, char& ready, const bool& start,
                 const bool& quit, const MakeTx& make_tx,
                 const WorkerInit& worker_init,
                 const WorkerAfterStart& worker_after_start) {
  // `Backoff` on the worker's stack so its lifetime covers `trans`.
  // Constructed even for protocols that don't use it (silo etc.) - the
  // ctor is one integer multiply, not worth a template branch.
  Backoff backoff(FLAGS_clocks_per_us);
  // C++17 guaranteed copy elision: `make_tx(thid, quit, backoff)`
  // returns a prvalue and is constructed directly into `trans`, so
  // `Tx` does not need to be move-constructible.
  Tx trans = make_tx(thid, quit, backoff);
  Workload workload;
  if constexpr (HasPrepare<Workload, Tx>) { workload.prepare(trans, nullptr); }

  worker_init(trans, thid);

  storeRelease(ready, static_cast<char>(1));
  while (!loadAcquire(start)) _mm_pause();
  worker_after_start(trans, thid);
  while (!loadAcquire(quit)) {
    workload.template run<Tx, TransactionStatus>(trans);
  }
}

} // namespace runner_detail

/**
 * @brief Run a workload-protocol binary end-to-end.
 *
 * Replaces the ~80 lines of `worker()` + `main()` boilerplate that
 * every `<wl>_<proto>.cc` used to carry. The caller still does:
 *   - gflags parse, `chkArg()` (or `init()` for oze);
 *   - `Workload::displayWorkloadParameter()` and `Workload::makeDB(...)`
 *     (these are workload-static and need protocol-specific
 *     `TupleInitParam` etc.);
 *   - any protocol-specific post-makeDB hook like Cicada's
 *     `MinWts.store(param->initial_wts + 2, ...)`;
 *   - `initResult(TotalThreadNum)`.
 *
 * Then this template handles the rest: barrier setup, thread spawn
 * (plus an optional BoMB dispatcher), the `FLAGS_extime` sleep loop,
 * join, result aggregation, and `displayAllResult`.
 *
 * @tparam Tx                Protocol's `TxExecutor`, constrained by
 *                           `TxExecutorLike`. A protocol that forgets
 *                           e.g. the `int64_t limit` overload of `scan`
 *                           fails here with a named diagnostic.
 * @tparam TransactionStatus Protocol's enum used by
 *                           `workload.run<Tx, TransactionStatus>(trans)`.
 * @tparam Workload          Workload template instance (`YcsbWorkload`,
 *                           `TPCCWorkload<Tuple, Param>`,
 *                           `BombWorkload<Tuple, Param>`, ...). If it
 *                           exposes `prepare(tx, nullptr)` it is called
 *                           per worker before the ready barrier.
 *
 * @param thread_num         Number of worker threads. Almost always the
 *                           protocol's `TotalThreadNum` global, except
 *                           tictoc which historically counted with
 *                           `FLAGS_thread_num` (numerically identical
 *                           for tictoc - it has no batch threads - but
 *                           the parameter is kept for output fidelity).
 * @param opts               See `RunnerOptions`.
 * @param make_tx            Constructs the worker's `Tx`. Signature
 *                           is `Tx(std::size_t thid, const bool& quit,
 *                           Backoff& backoff)`. silo-style executors
 *                           ignore `backoff`; cicada/ermia/mvto/oze/si-
 *                           style executors keep `backoff` by reference
 *                           and rely on its worker-thread lifetime,
 *                           which the runner guarantees by constructing
 *                           `Backoff` on the worker stack just before
 *                           calling the factory. The `quit` reference
 *                           is owned by the runner and outlives the
 *                           worker.
 * @param worker_init        Per-thread post-construction setup
 *                           (MasstreeWrapper thread_init,
 *                           setThreadAffinity, WAL log file open,
 *                           `gcob.decideFirstRange()` on the leader,
 *                           etc.). `NoOpHook` if the protocol needs
 *                           none.
 * @param worker_after_start Per-thread setup that must run after the
 *                           start barrier (`trans.epoch_timer_start
 *                           = rdtscp()` on the leader for silo/mocc,
 *                           `trans.gcstart_ = rdtscp()` on every
 *                           thread for ermia/si). `NoOpHook` if the
 *                           protocol does not need it.
 * @param bomb_dispatcher_fn Pointer to `BombWorkload<Tuple, void>::
 *                           request_dispatcher`, or nullptr. Only
 *                           consulted if `opts.enable_bomb_dispatcher`
 *                           is true and `FLAGS_bomb_mixed_mode` is set.
 */
template <typename Tx, typename TransactionStatus, typename Workload,
          typename MakeTx, typename WorkerInit = NoOpHook,
          typename WorkerAfterStart = NoOpHook>
requires TxExecutorLike<Tx>
void run(std::size_t thread_num, const RunnerOptions& opts,
         const MakeTx& make_tx, const WorkerInit& worker_init = WorkerInit{},
         const WorkerAfterStart& worker_after_start = WorkerAfterStart{},
         BombDispatcherFn bomb_dispatcher_fn = nullptr) {
  alignas(CACHE_LINE_SIZE) bool start = false;
  alignas(CACHE_LINE_SIZE) bool quit = false;

  const bool spawn_dispatcher =
      opts.enable_bomb_dispatcher && bomb_dispatcher_fn != nullptr;
  const std::size_t readys_size = thread_num + (spawn_dispatcher ? 1u : 0u);
  std::vector<char> readys(readys_size);

  std::vector<std::thread> thv;
  thv.reserve(readys_size);
  for (std::size_t i = 0; i < thread_num; ++i) {
    thv.emplace_back([&, thid = i]() {
      runner_detail::worker_body<Tx, TransactionStatus, Workload>(
          thid, readys[thid], start, quit, make_tx, worker_init,
          worker_after_start);
    });
  }
  if (spawn_dispatcher) {
    thv.emplace_back(bomb_dispatcher_fn, thread_num,
                     std::ref(readys[thread_num]), std::ref(start),
                     std::ref(quit));
  }

  waitForReady(readys);
  const std::uint64_t start_tsc = rdtscp();
  storeRelease(start, true);
  for (std::size_t i = 0; i < FLAGS_extime; ++i) { sleepMs(1000); }
  storeRelease(quit, true);
  for (auto& th : thv) th.join();
  const std::uint64_t end_tsc = rdtscp();

  // `actual_extime` matches the legacy formula exactly: TSC delta
  // converted to seconds via `FLAGS_clocks_per_us`, rounded to an
  // integer (the legacy entrypoints printed this as `long double`).
  const long double actual_extime = std::round(
      (end_tsc - start_tsc) /
      (static_cast<long double>(FLAGS_clocks_per_us) * ::powl(10.0L, 6.0L)));

  for (std::size_t i = 0; i < thread_num; ++i) {
    CCBenchResults[0].addLocalAllResult(CCBenchResults[i]);
    if (opts.display_per_tx) {
      CCBenchResults[0].addLocalPerTxResult(CCBenchResults[i], TxTypes);
    }
  }

  ShowOptParameters();
  if (opts.show_actual_extime) {
    std::cout << "actual_extime:\t" << actual_extime << std::endl;
  }
  if (opts.pass_op_num) {
    CCBenchResults[0].displayAllResult(FLAGS_clocks_per_us, FLAGS_extime,
                                       thread_num, opts.max_ope,
                                       opts.batch_max_ope);
  } else {
    CCBenchResults[0].displayAllResult(FLAGS_clocks_per_us, FLAGS_extime,
                                       thread_num);
  }
  if (opts.display_per_tx) {
    std::cout << "Details per transaction type:" << std::endl;
    CCBenchResults[0].displayPerTxResult(TxTypes);
  }
}

} // namespace ccbench
