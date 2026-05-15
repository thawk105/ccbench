#!/usr/bin/env bash
set -euo pipefail

cat <<'EOF'

============================================================
ccbench devcontainer is ready.

Build everything in one shot from the repo root:

  cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  cmake --build build -j

CMake's FetchContent fetches and builds the third-party deps
(masstree, mimalloc, googletest) at configure time.

Or build a single protocol target, e.g.:

  cmake --build build --target tpcc_silo.exe

Note: this image is linux/amd64. On Apple Silicon it runs under
QEMU emulation, so use it for development only -- benchmark
numbers from this environment are not meaningful.
============================================================
EOF
