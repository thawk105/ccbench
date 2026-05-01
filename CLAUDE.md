# CLAUDE.md

Context for Claude when working in this repository.

## Target environment

- **OS**: x86_64 Linux (Debian/Ubuntu). Not portable to macOS.
- **Why**: code uses x86 intrinsics (`__cpuid_count` in [include/cpu.hh](include/cpu.hh)) and Linux-only APIs (`sched_setaffinity`, `SYS_gettid`, `<linux/fs.h>` in [include/fileio.hh](include/fileio.hh)). Build files like [tpcc_silo/CMakeLists.txt](tpcc_silo/CMakeLists.txt) gate behavior on `CMAKE_SYSTEM_NAME MATCHES "Linux"`.
- **CI**: GitHub Actions on `ubuntu-latest` — see [.github/workflows/](.github/workflows/) for the canonical build steps.

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

Then build a protocol — each top-level directory (`silo/`, `cicada/`, `mocc/`, `tictoc/`, `ermia/`, `si/`, `ss2pl/`, `occ/`, `tpcc_silo/`) is independent:

```
cd silo && mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug .. && make -j
```

Note: `bootstrap_tbb.sh` references `third_party/tbb`, which is **not** registered as a submodule. Skip it unless you add tbb manually.

## Repository layout

- `include/` — shared headers (atomics, rwlock, zipf, masstree wrapper, etc.)
- `common/` — shared sources used across protocols
- `<protocol>/` — one directory per concurrency control protocol, each with its own Makefile or CMakeLists.txt
- `third_party/` — submodules: `masstree`, `mimalloc`, `googletest`, `spdlog`
- `build_tools/` — bootstrap scripts and `ubuntu.deps` (apt package list)
- `instruction/` — micro-benchmarks for individual instructions (cache, fetch_add, etc.)
