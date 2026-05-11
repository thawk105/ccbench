# Build

## Host requirements

CCBench targets x86_64 Linux (Debian/Ubuntu). It uses x86 intrinsics
(`__cpuid_count` in [include/cpu.hh](../include/cpu.hh)) and Linux-only APIs
(`sched_setaffinity`, `<linux/fs.h>` in [include/fileio.hh](../include/fileio.hh)),
so it does not build natively on macOS or other platforms. For development
on macOS, use the [devcontainer](#devcontainer-on-macos--non-ubuntu-hosts).

Install build dependencies (this is what CI uses):

```sh
sudo apt-get update
sudo apt-get install -y $(cat build_tools/ubuntu.deps)
```

## Third-party libraries

These produce static libraries under `third_party/` that the protocols link
against. Run them once after cloning:

```sh
./build_tools/bootstrap.sh             # third_party/masstree
./build_tools/bootstrap_mimalloc.sh    # third_party/mimalloc
./build_tools/bootstrap_googletest.sh  # third_party/googletest
```

A `bootstrap_tbb.sh` exists but `third_party/tbb` is **not** registered as a
submodule — skip it unless you add tbb manually.

If a binary fails to find mimalloc at runtime, add
`third_party/mimalloc/out/release/` to `LD_LIBRARY_PATH`.

## Building everything (top-level CMake)

The top-level [CMakeLists.txt](../CMakeLists.txt) drives all protocol subdirectories.
Standard out-of-source build:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Binaries land under `build/<protocol>/` and are named `<workload>_<protocol>.exe`,
e.g. `build/silo/tpcc_silo.exe`, `build/cicada/ycsb_cicada.exe`,
`build/mocc/bomb_mocc.exe`.

To build only one binary:

```sh
cmake --build build --target tpcc_silo.exe
```

For a debug build with sanitizers (the default), use `-DCMAKE_BUILD_TYPE=Debug`.
Sanitizer toggles live in the top-level `CMakeLists.txt` (`ENABLE_SANITIZER`,
`ENABLE_UB_SANITIZER`, `ENABLE_COVERAGE`).

## Per-protocol build

Each `<protocol>/CMakeLists.txt` is also invokable directly, but the recommended
flow is the top-level build above. See [protocols.md](protocols.md) for the
list of protocols and which workloads they support.

## Devcontainer (on macOS / non-Ubuntu hosts)

The repo ships a devcontainer at [.devcontainer/](../.devcontainer/) pinned
to `linux/amd64`:

1. Open the repo in VS Code and run **Dev Containers: Reopen in Container**.
2. The post-create hook runs `git submodule update --init --recursive` and
   prints the next-step commands.
3. Inside the container, run the three bootstrap scripts above and the
   top-level `cmake -S . -B build` build.

The image is published to `ghcr.io/thawk105/ccbench-devcontainer:latest` and
rebuilt by [.github/workflows/devcontainer-image.yml](../.github/workflows/devcontainer-image.yml)
whenever `.devcontainer/` changes.

> **On Apple Silicon this runs under QEMU emulation.** Editing, compiling,
> and correctness tests are fine. Do **not** report benchmark numbers from
> this environment — run measurements on real x86_64 Linux hardware.
