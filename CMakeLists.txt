cmake_minimum_required(VERSION 3.10)

project(ccbench LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# set_compile_options() — applied to ccbench_common here and to every
# protocol binary via ccbench_add_protocol() (see cmake/CompileOptions.cmake).
include(CompileOptions)
# Options defines ccbench_universal_definitions(); ProtocolHelpers also includes
# it, but ccbench_common (below) needs the universal defines too (see comment
# there), so pull it in early. include() is idempotent for these cache/function
# defs.
include(Options)

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
# Third-party dependencies (masstree / mimalloc / googletest) are pulled in
# via FetchContent — see cmake/ThirdParty.cmake. Exposes ccbench::masstree
# / ccbench::mimalloc / GTest::gtest{,_main}, which replace the static
# IMPORTED targets and raw third_party/*.a paths used previously.
# ---------------------------------------------------------------------------
include(ThirdParty)

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
# ccbench_common compiles common/result.cc, which defines and resize()s the
# global CCBenchResults (std::vector<Result>). The Result struct grows under
# `#if ADD_ANALYSIS` (and other universal flags can affect shared layout/behavior),
# so ccbench_common MUST see the same universal definitions as the protocol
# targets that link it. Without this, ccbench_common built Result with the
# default flags (e.g. ADD_ANALYSIS=0 -> small Result) while a protocol built with
# ADD_ANALYSIS=1 iterated CCBenchResults with the larger stride -> ODR violation /
# heap-buffer-overflow (e.g. BACK_OFF=1 + ADD_ANALYSIS=1 crashed in
# leaderBackoffWork's range-for over CCBenchResults).
ccbench_universal_definitions(_common_universal_defs)
target_compile_definitions(ccbench_common PRIVATE ${_common_universal_defs})
set_compile_options(ccbench_common)
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

# Instruction-cost microbenchmarks under microbench/. Opt-in and OFF by
# default so the regular build (and CI) stays exactly as it is; turn on
# with `cmake -S . -B build -DCCBENCH_BUILD_MICROBENCH=ON`.
option(CCBENCH_BUILD_MICROBENCH "build instruction-cost microbenchmarks" OFF)
if(CCBENCH_BUILD_MICROBENCH)
  add_subdirectory(microbench)
endif()
