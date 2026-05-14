cmake_minimum_required(VERSION 3.10)

project(ccbench LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

set(THIRD_PARTY_DIR "${PROJECT_SOURCE_DIR}/third_party/")

option(ENABLE_SANITIZER "enable sanitizer on debug build" ON)
option(ENABLE_UB_SANITIZER "enable undefined behavior sanitizer on debug build" OFF)
option(ENABLE_COVERAGE "enable coverage on debug build" OFF)

# Auto-enable ccache when available — local builds also benefit from
# the cross-target dedup we get in CI. Override with -DCCBENCH_CCACHE=OFF.
option(CCBENCH_CCACHE "use ccache as compiler launcher when found" ON)
if(CCBENCH_CCACHE)
  find_program(CCACHE_PROGRAM ccache)
  if(CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER   "${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    message(STATUS "ccache enabled: ${CCACHE_PROGRAM}")
  endif()
endif()

find_package(Threads REQUIRED)
find_package(gflags REQUIRED)
find_package(glog REQUIRED)
find_package(Boost COMPONENTS filesystem)

# ---------------------------------------------------------------------------
# Imported targets for the bootstrap-built third-party static libraries.
# Replaces the raw ${THIRD_PARTY_DIR}/.../*.a paths previously copy-pasted
# into every protocol's target_link_libraries(...).
# ---------------------------------------------------------------------------
add_library(ccbench::masstree STATIC IMPORTED GLOBAL)
set_target_properties(ccbench::masstree PROPERTIES
  IMPORTED_LOCATION "${THIRD_PARTY_DIR}/masstree/libkohler_masstree_json.a")

add_library(ccbench::mimalloc STATIC IMPORTED GLOBAL)
set_target_properties(ccbench::mimalloc PROPERTIES
  IMPORTED_LOCATION "${THIRD_PARTY_DIR}/mimalloc/out/release/libmimalloc.a")

# ---------------------------------------------------------------------------
# common/ as a real shared static library — replaces the per-binary
# duplication that compiled common/result.cc + common/util.cc once per
# target (~28 duplicate compilations across all protocols × workloads).
# Protocols just `target_link_libraries(<bin> PRIVATE ccbench_common)`.
# ---------------------------------------------------------------------------
add_library(ccbench_common STATIC
  common/result.cc
  common/util.cc
)
target_compile_features(ccbench_common PUBLIC cxx_std_20)
target_include_directories(ccbench_common PUBLIC
  ${CMAKE_SOURCE_DIR}
)
target_link_libraries(ccbench_common PUBLIC
  Threads::Threads
  Boost::filesystem
  gflags::gflags
  glog::glog
)

# All concurrency-control protocols live under cc/. Each one's
# CMakeLists.txt is just a single ccbench_add_protocol(...) call.
include(ProtocolHelpers)
foreach(_proto cicada d2pl ermia mocc mvto oze si silo ss2pl tictoc)
  add_subdirectory(cc/${_proto})
endforeach()

# Dump the protocol × workload support matrix into the build dir, so the
# table in CLAUDE.md / docs/protocols.md can be refreshed mechanically.
ccbench_write_protocol_matrix("${CMAKE_BINARY_DIR}/PROTOCOL_MATRIX.md")
