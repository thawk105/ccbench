# Architecture

リポジトリの全体像と、各プロトコルが従う内部規約をまとめる。ビルド手順は [build.md](build_en.md)、プロトコル × ワークロードの対応表は [protocols.md](protocols_en.md)、ワークロード仕様は [workloads.md](workloads_en.md) を参照。

## What this repo is

CCBench re-implements major in-memory concurrency-control protocols on a common substrate so they can be compared on identical workloads (Tanabe et al., VLDB 2020). The repository now also bundles the **TPC-C** and **BoMB** workloads and several extra protocols, contributed by [@jnmt](https://github.com/jnmt) on the `vldb-paper` branch and merged here.

Three workloads:

- **TPC-C** — the standard OLTP benchmark
- **YCSB** — key-value workload (read/write ratio, zipfian skew, etc.)
- **BoMB** — Bill-of-Materials Benchmark (long-running product costing + short OLTP txns)

Workload specs are in [workloads.md](workloads_en.md).

## Repository layout

- [include/](../include/) — shared headers (atomics, rwlock, zipf, masstree wrapper, etc.)
- [include/tpcc/](../include/tpcc/) — unified TPC-C framework (tables, queries, 5 transactions)
- [include/ycsb.hh](../include/ycsb.hh), [include/bomb.hh](../include/bomb.hh), [include/bomb_pessimistic.hh](../include/bomb_pessimistic.hh), [include/dbomb_deterministic.hh](../include/dbomb_deterministic.hh), [include/sbomb_deterministic.hh](../include/sbomb_deterministic.hh), [include/workload.hh](../include/workload.hh) — workload entry points
- [common/](../common/) — shared sources used across protocols
- [cc/](../cc/) — one subdirectory per concurrency control protocol (`cc/silo/`, `cc/cicada/`, …), each with a single-call `CMakeLists.txt` and the `<workload>_<protocol>.cc` entry points
- [third_party/](../third_party/) — submodules: `masstree`, `mimalloc`, `googletest`, `spdlog`
- [build_tools/](../build_tools/) — bootstrap scripts and `ubuntu.deps` (apt package list)
- [cmake/](../cmake/) — shared CMake modules: `CompileOptions.cmake`, [Options.cmake](../cmake/Options.cmake) (universal `-D` flags), [ProtocolHelpers.cmake](../cmake/ProtocolHelpers.cmake) (`ccbench_add_protocol`)
- [docs/](.) — this directory
- [microbench/](../microbench/) — microbenchmarks measuring the cost of instructions / operations (`fetch_add`, `membench`, etc.); not built by default, opt in with `-DCCBENCH_BUILD_MICROBENCH=ON`

## Common transaction API (compile-time enforced)

All built protocols expose the same `TxExecutor` API expected by the workload templates in [include/tpcc.hh](../include/tpcc.hh), [include/bomb.hh](../include/bomb.hh), [include/ycsb.hh](../include/ycsb.hh):

```cpp
Status read(Storage s, std::string_view key, TupleBody** body);
Status update(Storage s, std::string_view key, TupleBody&& body);   // SQL UPDATE; returns WARN_NOT_FOUND if row absent
Status insert(Storage s, std::string_view key, TupleBody&& body);   // SQL INSERT
Status delete_record(Storage s, std::string_view key);
Status scan(..., std::vector<TupleBody*>& result);
Status scan(..., std::vector<TupleBody*>& result, int64_t limit);   // limit form is required (TPC-C uses it)
bool   commit();
void   abort();
```

This contract is **enforced at compile time** by [include/tx_executor_concept.hh](../include/tx_executor_concept.hh). Each protocol's `transaction.hh` ends with `static_assert(TxExecutorLike<TxExecutor>);`, so a missing or wrong-signature method fails the protocol's own build with a named diagnostic — no more "ran fine, then crashed inside TPC-C delivery because the limit overload of `scan` was missing".

The check uses an `is_detected`-style SFINAE helper instead of a C++20 `concept` because the bundled `masstree-beta` upstream still calls `std::allocator::construct/destroy` (removed in C++20), pinning the whole tree to C++17.

The semantics of `tx.read` (Status return, miss-handling) are non-obvious and are written up as a coding rule in [coding-conventions.md § C++](coding-conventions_en.md).

## Per-thread GC + cross-thread Tuple references

`cc/ermia/` and `cc/si/` keep a per-thread `gcq_for_version_` (entries reference `Tuple*` via `rcdptr_`) plus a per-thread `gcq_for_record_` (`Tuple*` itself). A `Tuple` deleted by Thread A goes only into A's `gcq_for_record_`, but Thread B's `gcq_for_version_` may still reference it. To avoid UAF in `gcRecord`, both protocols use an EBR-style guard: each thread publishes the smallest cstamp in its `gcq_for_version_` to `MinQueuedCstamp[thid]` on push (in commit) and after the pop loop in `gcVersion`; `gcRecord` only frees a Tuple whose delete cstamp is strictly less than the global min. **If you change either GC queue's push/pop sites, keep the publish call in sync** — see comments in [cc/si/garbage_collection.cc](../cc/si/garbage_collection.cc) and [cc/ermia/garbage_collection.cc](../cc/ermia/garbage_collection.cc).
