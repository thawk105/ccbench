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

Then build a protocol, e.g.:

  cd silo && mkdir -p build && cd build
  cmake -DCMAKE_BUILD_TYPE=Debug .. && make -j

Note: this image is linux/amd64. On Apple Silicon it runs under
QEMU emulation, so use it for development only -- benchmark
numbers from this environment are not meaningful.
============================================================
EOF
