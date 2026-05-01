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

## Building a protocol

Each protocol directory is independent. The standard pattern is:

```sh
cd silo && mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j
./silo.exe -help
```

`occ/` uses a plain Makefile instead of CMake. See each protocol's own
`README.md` for protocol-specific notes (build flags, example invocations).

## Devcontainer (on macOS / non-Ubuntu hosts)

The repo ships a devcontainer at [.devcontainer/](../.devcontainer/) pinned
to `linux/amd64`:

1. Open the repo in VS Code and run **Dev Containers: Reopen in Container**.
2. The post-create hook runs `git submodule update --init --recursive`.
3. Inside the container, run the three bootstrap scripts above and build
   any protocol normally.

The image is published to `ghcr.io/thawk105/ccbench-devcontainer:latest` and
rebuilt by [.github/workflows/devcontainer-image.yml](../.github/workflows/devcontainer-image.yml)
whenever `.devcontainer/` changes.

> **On Apple Silicon this runs under QEMU emulation.** Editing, compiling,
> and correctness tests are fine. Do **not** report benchmark numbers from
> this environment — run measurements on real x86_64 Linux hardware.
