# CLAUDE.md

Context for Claude when working in this repository.

## What this repo is

This is an **extended CCBench**, originally based on jnmt's vldb-paper branch. It provides a benchmark platform for concurrency-control protocols, supporting three workloads:

- **TPC-C** — the standard OLTP benchmark
- **YCSB** — key-value workload (read/write ratio, zipfian skew, etc.)
- **BoMB** — Bill-of-Materials Benchmark (long-running product costing + short OLTP txns), described in detail in [README.md](README.md)

Each workload runs across multiple concurrency-control protocols, giving cross-protocol comparison on identical workloads.

## Target environment

- **OS**: x86_64 Linux (Debian/Ubuntu). Not portable to macOS.
- **Why**: code uses x86 intrinsics (`__cpuid_count` in [include/cpu.hh](include/cpu.hh)) and Linux-only APIs (`sched_setaffinity`, `SYS_gettid`, `<linux/fs.h>` in [include/fileio.hh](include/fileio.hh)).
- **CI**: GitHub Actions on `ubuntu-latest` — see [.github/workflows/build.yml](.github/workflows/build.yml) for the canonical build steps.

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

Note: `bootstrap_tbb.sh` references `third_party/tbb`, which is **not** registered as a submodule. Skip it unless you add tbb manually.

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
| `ss2pl/`  | — | — | ✓ | — |
| `d2pl/`   | — | — | ✓ | sBoMB + dBoMB |
| `mvto/`   | — | — | ✓ | — |

**Not built by default** (commented out in top-level [CMakeLists.txt](CMakeLists.txt)):
- `si/` — older snapshot-isolation protocol; build is left in tree but excluded.
- `occ/` — present in tree but not wired into the top-level build.

The deleted standalone `tpcc_silo/` directory has been replaced by `silo/tpcc_silo.cc`, which uses the unified TPC-C framework in [include/tpcc/](include/tpcc/).

## Repository layout

- [include/](include/) — shared headers (atomics, rwlock, zipf, masstree wrapper, etc.)
- [include/tpcc/](include/tpcc/) — unified TPC-C framework (tables, queries, 5 transactions)
- [include/ycsb.hh](include/ycsb.hh), [include/bomb.hh](include/bomb.hh), [include/workload.hh](include/workload.hh) — workload entry points
- [common/](common/) — shared sources used across protocols
- `<protocol>/` — one directory per concurrency control protocol, each with its own `CMakeLists.txt` and `<workload>_<protocol>.cc` entry points
- [third_party/](third_party/) — submodules: `masstree`, `mimalloc`, `googletest`, `spdlog`
- [build_tools/](build_tools/) — bootstrap scripts and `ubuntu.deps` (apt package list)
- [cmake/](cmake/) — shared CMake modules (e.g. `CompileOptions.cmake`)
- [instruction/](instruction/) — micro-benchmarks for individual instructions (cache, fetch_add, etc.)
