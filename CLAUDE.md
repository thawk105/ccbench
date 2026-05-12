# CLAUDE.md

Context for Claude when working in this repository.

## What this repo is

CCBench re-implements major in-memory concurrency-control protocols on a common substrate so they can be compared on identical workloads (Tanabe et al., VLDB 2020). The repository now also bundles the **TPC-C** and **BoMB** workloads and several extra protocols, contributed by [@jnmt](https://github.com/jnmt) on the `vldb-paper` branch and merged here.

Three workloads:

- **TPC-C** — the standard OLTP benchmark
- **YCSB** — key-value workload (read/write ratio, zipfian skew, etc.)
- **BoMB** — Bill-of-Materials Benchmark (long-running product costing + short OLTP txns)

Workload specs are in [docs/workloads.md](docs/workloads.md).

## Target environment

- **OS**: x86_64 Linux (Debian/Ubuntu). Not portable to macOS.
- **Why**: code uses x86 intrinsics (`__cpuid_count` in [include/cpu.hh](include/cpu.hh)) and Linux-only APIs (`sched_setaffinity`, `SYS_gettid`, `<linux/fs.h>` in [include/fileio.hh](include/fileio.hh)).
- **CI**: GitHub Actions on `ubuntu-latest` — see [.github/workflows/build.yml](.github/workflows/build.yml). It triggers on push to any branch and on PRs; ccache + apt + bootstrap output are all cached.

## Working from macOS

Use the devcontainer at [.devcontainer/](.devcontainer/) (`Dev Containers: Reopen in Container` in VS Code). It pins `linux/amd64`, so on Apple Silicon it runs under QEMU emulation.

- ✅ Editing, compiling, unit tests, correctness checks
- ❌ **Performance benchmarks** — numbers from QEMU are meaningless. Use a real x86_64 Linux box for any measurement.

## Build flow

After cloning (or after `Reopen in Container` finishes submodule init):

```
./build_tools/bootstrap.sh             # masstree
./build_tools/bootstrap_mimalloc.sh    # mimalloc
./build_tools/bootstrap_googletest.sh  # googletest
```

Then build everything in one shot from the repo root using the top-level [CMakeLists.txt](CMakeLists.txt):

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

Or build one specific target:

```
cmake --build build --target tpcc_silo.exe
```

The top-level CMake auto-enables **ccache** as a compiler launcher if `ccache` is on PATH, deduplicating compilations across the ~28 binaries (full warm rebuild ≈ 3 sec vs 30+ sec cold). Disable with `-DCCBENCH_CCACHE=OFF`.

`bootstrap_tbb.sh` references `third_party/tbb`, which is **not** registered as a submodule. Skip it unless you add tbb manually.

## Protocols and workload coverage

The top-level CMake builds these protocols. Each produces one binary per supported workload, named `<workload>_<protocol>.exe`.

| Protocol | YCSB | TPC-C | BoMB | sBoMB / dBoMB |
|---|:-:|:-:|:-:|:-:|
| `silo/`   | ✓ | ✓ | ✓ | sBoMB |
| `mocc/`   | ✓ | ✓ | ✓ | sBoMB |
| `cicada/` | ✓ | ✓ | ✓ | sBoMB |
| `ermia/`  | ✓ | ✓ | ✓ | sBoMB |
| `tictoc/` | ✓ | ✓ | ✓ | sBoMB |
| `oze/`    | ✓ | ✓ | ✓ | — |
| `si/`     | ✓ | ✓ | ✓ | sBoMB |
| `ss2pl/`  | — | ✓ | ✓ | — |
| `mvto/`   | — | ✓ | ✓ | — |
| `d2pl/`   | — | — | ✓ | sBoMB + dBoMB |

**Not built by default** ([CMakeLists.txt](CMakeLists.txt) keeps the line commented):
- `occ/` — present in tree but uses a plain Makefile, not wired into the CMake tree.

`si/` is a fresh implementation derived from `ermia/` by stripping the SSN (Serial Safety Net) layer; the original legacy `si/` (uint64-key API) was replaced. `d2pl/` deliberately doesn't support TPC-C — its deterministic locking model needs pre-declared `lock_entries_`, which the optimistic-style TPC-C templates don't provide.

## Implementation notes

### Common transaction API (compile-time enforced)

All built protocols expose the same `TxExecutor` API expected by the workload templates in [include/tpcc.hh](include/tpcc.hh), [include/bomb.hh](include/bomb.hh), [include/ycsb.hh](include/ycsb.hh):

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

This contract is **enforced at compile time** by [include/tx_executor_concept.hh](include/tx_executor_concept.hh). Each protocol's `transaction.hh` ends with `static_assert(TxExecutorLike<TxExecutor>);`, so a missing or wrong-signature method fails the protocol's own build with a named diagnostic — no more "ran fine, then crashed inside TPC-C delivery because the limit overload of `scan` was missing".

The check uses an `is_detected`-style SFINAE helper instead of a C++20 `concept` because the bundled `masstree-beta` upstream still calls `std::allocator::construct/destroy` (removed in C++20), pinning the whole tree to C++17.

### `tx.read` returns Status — *check it*

`tx.read` returns `Status::OK` on hit and `Status::WARN_NOT_FOUND` on miss. On miss, `*body` is **left unchanged** (it does not get nullified). Checking only `tx.status_ == TransactionStatus::aborted` is not enough — a stale `body` pointer from a previous read will silently get dereferenced, which manifests as a `HeapObject::cast_to` assertion under Debug+ASan and as garbage data in Release. Always:

```cpp
Status stat = tx.read(s, key, &body);
if (tx.status_ == TransactionStatus::aborted) return false;
if (stat != Status::OK) return false;   // do not skip this
```

### Per-thread GC + cross-thread Tuple references

`ermia/` and `si/` keep a per-thread `gcq_for_version_` (entries reference `Tuple*` via `rcdptr_`) plus a per-thread `gcq_for_record_` (`Tuple*` itself). A `Tuple` deleted by Thread A goes only into A's `gcq_for_record_`, but Thread B's `gcq_for_version_` may still reference it. To avoid UAF in `gcRecord`, both protocols use an EBR-style guard: each thread publishes the smallest cstamp in its `gcq_for_version_` to `MinQueuedCstamp[thid]` on push (in commit) and after the pop loop in `gcVersion`; `gcRecord` only frees a Tuple whose delete cstamp is strictly less than the global min. **If you change either GC queue's push/pop sites, keep the publish call in sync** — see comments in [si/garbage_collection.cc](si/garbage_collection.cc) and [ermia/garbage_collection.cc](ermia/garbage_collection.cc).

### Build modes for development

- **Debug+ASan** (the default top-level Debug build) is the right mode for correctness work — most TPC-C bugs we have caught (use-after-free in `get_and_update_*`, the `cast_to<Order>` assertion, the gcRecord UAF) showed up there first and were invisible under pure Release.
- **Release** is for benchmark numbers only. CI builds Release without sanitizer (`-DENABLE_SANITIZER=OFF`) — it does not run binaries, just verifies they compile.

## Repository layout

- [include/](include/) — shared headers (atomics, rwlock, zipf, masstree wrapper, etc.)
- [include/tpcc/](include/tpcc/) — unified TPC-C framework (tables, queries, 5 transactions)
- [include/ycsb.hh](include/ycsb.hh), [include/bomb.hh](include/bomb.hh), [include/bomb_pessimistic.hh](include/bomb_pessimistic.hh), [include/dbomb_deterministic.hh](include/dbomb_deterministic.hh), [include/sbomb_deterministic.hh](include/sbomb_deterministic.hh), [include/workload.hh](include/workload.hh) — workload entry points
- [common/](common/) — shared sources used across protocols
- `<protocol>/` — one directory per concurrency control protocol, each with its own `CMakeLists.txt` and `<workload>_<protocol>.cc` entry points
- [third_party/](third_party/) — submodules: `masstree`, `mimalloc`, `googletest`, `spdlog`
- [build_tools/](build_tools/) — bootstrap scripts and `ubuntu.deps` (apt package list)
- [cmake/](cmake/) — shared CMake modules (e.g. `CompileOptions.cmake`)
- [docs/](docs/) — `build.md`, `workloads.md`, `protocols.md`, `runtime-args.md`
- [instruction/](instruction/) — micro-benchmarks for individual instructions (cache, fetch_add, etc.)
