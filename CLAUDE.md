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
cmake --build build --target tpcc_silo.exe   # binary lands in build/cc/silo/
```

The top-level CMake auto-enables **ccache** as a compiler launcher if `ccache` is on PATH, deduplicating compilations across the ~28 binaries (full warm rebuild ≈ 3 sec vs 30+ sec cold). Disable with `-DCCBENCH_CCACHE=OFF`.

`bootstrap_tbb.sh` references `third_party/tbb`, which is **not** registered as a submodule. Skip it unless you add tbb manually.

## Protocols and workload coverage

All CC protocols live under [cc/](cc/) — `cc/silo/`, `cc/cicada/`, etc. Each one's [CMakeLists.txt](cc/silo/CMakeLists.txt) is a single declarative `ccbench_add_protocol(...)` call (see [cmake/ProtocolHelpers.cmake](cmake/ProtocolHelpers.cmake)); the top-level [CMakeLists.txt](CMakeLists.txt) iterates over them. Each produces one binary per supported workload, named `<workload>_<protocol>.exe`, landing in `build/cc/<protocol>/`.

The support matrix is **auto-generated** at configure time from the `WORKLOADS` argument of every `ccbench_add_protocol(...)` call into `build/PROTOCOL_MATRIX.md`. Don't hand-edit a copy of it here — re-run cmake and paste the file contents if you need a snapshot:

| Protocol | YCSB | TPC-C | BoMB | sBoMB / dBoMB |
|---|:-:|:-:|:-:|:-:|
| `cc/silo/`   | ✓ | ✓ | ✓ | sBoMB |
| `cc/mocc/`   | ✓ | ✓ | ✓ | sBoMB |
| `cc/cicada/` | ✓ | ✓ | ✓ | sBoMB |
| `cc/ermia/`  | ✓ | ✓ | ✓ | sBoMB |
| `cc/tictoc/` | ✓ | ✓ | ✓ | sBoMB |
| `cc/oze/`    | ✓ | ✓ | ✓ | — |
| `cc/si/`     | ✓ | ✓ | ✓ | sBoMB |
| `cc/ss2pl/`  | — | ✓ | ✓ | — |
| `cc/mvto/`   | — | ✓ | ✓ | — |
| `cc/d2pl/`   | — | — | ✓ | sBoMB + dBoMB |

**Not built by default** ([CMakeLists.txt](CMakeLists.txt) keeps the line commented):
- [cc/occ/](cc/occ/) — present in tree but uses a plain Makefile on the legacy uint64-key API, not wired into the CMake tree.

`cc/si/` is a fresh implementation derived from `cc/ermia/` by stripping the SSN (Serial Safety Net) layer; the original legacy `si/` (uint64-key API) was replaced. `cc/d2pl/` deliberately doesn't support TPC-C — its deterministic locking model needs pre-declared `lock_entries_`, which the optimistic-style TPC-C templates don't provide.

### Adding or tuning a protocol

A protocol's CMakeLists.txt is a single function call:

```cmake
ccbench_add_protocol(<name>
  SOURCES   <shared .cc files>                # do NOT list workload entry points
  WORKLOADS ycsb tpcc bomb sbomb              # any subset of these tags
  OPTIONS                                     # protocol-specific -D defines
    FOO=${CCBENCH_FOO}
    BAR                                       # bare flag = `-DBAR` (no value)
)
```

Universal `-D` flags (`KEY_SIZE`, `VAL_SIZE`, `BACK_OFF`, `ADD_ANALYSIS`, `MASSTREE_USE`) and the link against `ccbench_common` + `ccbench::masstree` + `ccbench::mimalloc` are added automatically. New cache options go in [cmake/Options.cmake](cmake/Options.cmake). An `OPTIONS` entry whose value is empty is dropped — same as the old `remove_definitions(-DFLAG)` path.

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

`cc/ermia/` and `cc/si/` keep a per-thread `gcq_for_version_` (entries reference `Tuple*` via `rcdptr_`) plus a per-thread `gcq_for_record_` (`Tuple*` itself). A `Tuple` deleted by Thread A goes only into A's `gcq_for_record_`, but Thread B's `gcq_for_version_` may still reference it. To avoid UAF in `gcRecord`, both protocols use an EBR-style guard: each thread publishes the smallest cstamp in its `gcq_for_version_` to `MinQueuedCstamp[thid]` on push (in commit) and after the pop loop in `gcVersion`; `gcRecord` only frees a Tuple whose delete cstamp is strictly less than the global min. **If you change either GC queue's push/pop sites, keep the publish call in sync** — see comments in [cc/si/garbage_collection.cc](cc/si/garbage_collection.cc) and [cc/ermia/garbage_collection.cc](cc/ermia/garbage_collection.cc).

### Build modes for development

- **Debug+ASan** (the default top-level Debug build) is the right mode for correctness work — most TPC-C bugs we have caught (use-after-free in `get_and_update_*`, the `cast_to<Order>` assertion, the gcRecord UAF) showed up there first and were invisible under pure Release.
- **Release** is for benchmark numbers only. CI builds Release without sanitizer (`-DENABLE_SANITIZER=OFF`) — it does not run binaries, just verifies they compile.

### Compiler-version mismatch with CI

CI runs on `ubuntu-latest` (currently Ubuntu 24.04 → **GCC 13**). The default devcontainer ships **GCC 11**. The two compilers disagree on `-Wmaybe-uninitialized` (and likely other flow-sensitive warnings): GCC 13 catches false-positive-prone cases that GCC 11 lets through.

When working on the phased `-Werror` cleanup (#43) — or anything else that promotes a warning to error — **verify on GCC 13 locally before pushing**. Three rounds of CI red on PR #44 (`-Wmaybe-uninitialized`) were avoidable by doing this once. Options:

```sh
# Option 1: PPA (one-time setup, fastest iteration afterwards)
sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
sudo apt-get update && sudo apt-get install -y gcc-13 g++-13
CC=gcc-13 CXX=g++-13 cmake -S . -B build-gcc13 -DCMAKE_BUILD_TYPE=Release -DENABLE_SANITIZER=OFF
cmake --build build-gcc13 -j
```

```sh
# Option 2: Docker (no host changes; slower because deps re-install each run)
docker run --rm -v "$PWD":/ccbench -w /ccbench ubuntu:24.04 bash -c '
  apt-get update && apt-get install -y --no-install-recommends \
    $(cat build_tools/ubuntu.deps) build-essential pkg-config && \
  cmake -S . -B build-gcc13 -DCMAKE_BUILD_TYPE=Release -DENABLE_SANITIZER=OFF && \
  cmake --build build-gcc13 -j
'
```

Pushing a `-Werror=<flag>` promotion without a GCC 13 build first counts as "didn't actually verify."

## Repository layout

- [include/](include/) — shared headers (atomics, rwlock, zipf, masstree wrapper, etc.)
- [include/tpcc/](include/tpcc/) — unified TPC-C framework (tables, queries, 5 transactions)
- [include/ycsb.hh](include/ycsb.hh), [include/bomb.hh](include/bomb.hh), [include/bomb_pessimistic.hh](include/bomb_pessimistic.hh), [include/dbomb_deterministic.hh](include/dbomb_deterministic.hh), [include/sbomb_deterministic.hh](include/sbomb_deterministic.hh), [include/workload.hh](include/workload.hh) — workload entry points
- [common/](common/) — shared sources used across protocols
- [cc/](cc/) — one subdirectory per concurrency control protocol (`cc/silo/`, `cc/cicada/`, …), each with a single-call `CMakeLists.txt` and the `<workload>_<protocol>.cc` entry points
- [third_party/](third_party/) — submodules: `masstree`, `mimalloc`, `googletest`, `spdlog`
- [build_tools/](build_tools/) — bootstrap scripts and `ubuntu.deps` (apt package list)
- [cmake/](cmake/) — shared CMake modules: `CompileOptions.cmake`, [Options.cmake](cmake/Options.cmake) (universal `-D` flags), [ProtocolHelpers.cmake](cmake/ProtocolHelpers.cmake) (`ccbench_add_protocol`)
- [docs/](docs/) — `build.md`, `workloads.md`, `protocols.md`, `runtime-args.md`
- [instruction/](instruction/) — micro-benchmarks for individual instructions (cache, fetch_add, etc.)
