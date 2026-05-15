# Build

## Host requirements

CCBench targets x86_64 Linux (Debian/Ubuntu). It uses x86 intrinsics
(`__cpuid_count` in [include/cpu.hh](../include/cpu.hh)) and Linux-only APIs
(`sched_setaffinity`, `SYS_gettid`, `<linux/fs.h>` in [include/fileio.hh](../include/fileio.hh)),
so it does not build natively on macOS or other platforms. For development
on macOS, use the [devcontainer](#devcontainer-on-macos--non-ubuntu-hosts).

CI runs on GitHub Actions `ubuntu-latest` — see
[.github/workflows/build.yml](../.github/workflows/build.yml). It triggers
on push to any branch and on PRs; ccache and apt are cached.

Install build dependencies (this is what CI uses):

```sh
sudo apt-get update
sudo apt-get install -y $(cat build_tools/ubuntu.deps)
```

## Building everything (top-level CMake)

The top-level [CMakeLists.txt](../CMakeLists.txt) drives all protocol subdirectories.
Standard out-of-source build:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Binaries land under `build/cc/<protocol>/` and are named `<workload>_<protocol>.exe`,
e.g. `build/cc/silo/tpcc_silo.exe`, `build/cc/cicada/ycsb_cicada.exe`,
`build/cc/mocc/bomb_mocc.exe`.

To build only one binary:

```sh
cmake --build build --target tpcc_silo.exe
```

For a debug build with sanitizers (the default), use `-DCMAKE_BUILD_TYPE=Debug`.
Sanitizer toggles live in the top-level `CMakeLists.txt` (`ENABLE_SANITIZER`,
`ENABLE_UB_SANITIZER`, `ENABLE_COVERAGE`).

## Per-protocol build

Each `cc/<protocol>/CMakeLists.txt` is a single declarative
`ccbench_add_protocol(...)` call — see [cmake/ProtocolHelpers.cmake](../cmake/ProtocolHelpers.cmake)
for the function definition and [cmake/Options.cmake](../cmake/Options.cmake)
for the universal build-time tunables (`CCBENCH_KEY_SIZE`, `CCBENCH_BACK_OFF`,
etc.) that can be overridden on the cmake command line. See
[protocols.md](protocols_en.md) for the list of protocols and which workloads
they support.

The top-level CMake auto-enables **ccache** as a compiler launcher if `ccache`
is on PATH, deduplicating compilations across the ~34 binaries (full warm
rebuild ≈ 3 sec vs 30+ sec cold). Disable with `-DCCBENCH_CCACHE=OFF`.

The instruction-cost microbenchmarks ([microbench/](../microbench/)) that
measure the cost of `include/` components are not built by default; opt in
with `-DCCBENCH_BUILD_MICROBENCH=ON`.

## Build modes for development

- **Debug+ASan** (the default top-level Debug build) is the right mode for
  correctness work — most TPC-C bugs we have caught (use-after-free in
  `get_and_update_*`, the `cast_to<Order>` assertion, the gcRecord UAF)
  showed up there first and were invisible under pure Release.
- **Release** is for benchmark numbers only. CI builds Release without
  sanitizer (`-DENABLE_SANITIZER=OFF`) — it does not run binaries, just
  verifies they compile.

## Compiler version

Both the devcontainer (`ubuntu:24.04` base, see
[.devcontainer/Dockerfile](../.devcontainer/Dockerfile)) and CI
(`ubuntu-latest`) ship **GCC 13**, so what builds in the devcontainer also
builds in CI. This wasn't always true — see #44, where GCC 11 in an older
devcontainer disagreed with CI's GCC 13 on `-Wmaybe-uninitialized` and
burned three CI cycles before the gap was closed.

## Devcontainer (on macOS / non-Ubuntu hosts)

The repo ships a devcontainer at [.devcontainer/](../.devcontainer/) pinned
to `linux/amd64`:

1. Open the repo in VS Code and run **Dev Containers: Reopen in Container**.
2. Inside the container, run the top-level
   `cmake -S . -B build && cmake --build build`. CMake's FetchContent
   fetches and builds the third-party dependencies at configure time.

The image is published to `ghcr.io/thawk105/ccbench-devcontainer:latest` and
rebuilt by [.github/workflows/devcontainer-image.yml](../.github/workflows/devcontainer-image.yml)
whenever `.devcontainer/` changes.

> **On Apple Silicon this runs under QEMU emulation.** Editing, compiling,
> and correctness tests are fine. Do **not** report benchmark numbers from
> this environment — run measurements on real x86_64 Linux hardware.
