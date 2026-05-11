#!/usr/bin/env bash
set -euo pipefail

git submodule update --init --recursive

cat <<'EOF'

============================================================
ccbench devcontainer is ready.

Next steps (run these inside the container):

  ./build_tools/bootstrap.sh             # build masstree
  ./build_tools/bootstrap_mimalloc.sh    # build mimalloc
  ./build_tools/bootstrap_googletest.sh  # build googletest

Then build everything in one shot from the repo root:

  cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  cmake --build build -j

Or build a single protocol target, e.g.:

  cmake --build build --target tpcc_silo.exe

Note: this image is linux/amd64. On Apple Silicon it runs under
QEMU emulation, so use it for development only -- benchmark
numbers from this environment are not meaningful.
============================================================
EOF
