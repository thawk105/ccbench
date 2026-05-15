# cmake/ThirdParty.cmake
#
# Centralised FetchContent declarations for ccbench's third-party
# dependencies. Replaces tracked submodules under third_party/ +
# the per-library build_tools/bootstrap_*.sh scripts.
#
# Targets exposed to the rest of the build:
#   ccbench::masstree   — static archive built from upstream sources
#                         (libkohler_masstree_json.a; same 11 objects the
#                         old build_tools/bootstrap.sh produced)
#   ccbench::mimalloc   — alias for the upstream mimalloc-static target
#   GTest::gtest /
#   GTest::gtest_main   — provided directly by upstream googletest
#
# All revisions are pinned to commit SHAs / tags so the build is
# reproducible and FetchContent never silently follows a moving branch.

include(FetchContent)
include(ExternalProject)

# --- masstree --------------------------------------------------------------
#
# Upstream masstree-beta is not a CMake project (autotools + hand-rolled
# Makefile). We mirror the old build_tools/bootstrap.sh exactly:
#   ./bootstrap.sh && ./configure --disable-assertions
#   make CXXFLAGS='-g -W -Wall -O3 -fPIC'
#   ar cr libkohler_masstree_json.a <11 objects> && ranlib …
# done at configure time via ExternalProject_Add, then exposed as a
# STATIC IMPORTED target so the rest of the CMake graph treats it the
# same way as before.
#
# The fork URL + commit SHA come from the previous .gitmodules /
# `git submodule status` (see issue #97).

set(CCBENCH_MASSTREE_REPO  "https://github.com/thawk105/masstree-beta.git")
set(CCBENCH_MASSTREE_TAG   "b3c5d054b66b08374d7a6ff5a0faeaf28b041a38")
set(CCBENCH_MIMALLOC_REPO  "https://github.com/microsoft/mimalloc.git")
set(CCBENCH_MIMALLOC_TAG   "v2.3.2")  # commit 02a2f5d, matches old submodule
set(CCBENCH_GOOGLETEST_REPO "https://github.com/google/googletest.git")
set(CCBENCH_GOOGLETEST_TAG  "f8d7d77c06936315286eb55f8de22cd23c188571")

FetchContent_Declare(
  masstree
  GIT_REPOSITORY "${CCBENCH_MASSTREE_REPO}"
  GIT_TAG        "${CCBENCH_MASSTREE_TAG}"
  GIT_SHALLOW    FALSE   # SHA pin needs the full object, not a tag-only fetch
)

# Populate() leaves us with masstree_SOURCE_DIR / masstree_BINARY_DIR but
# does NOT add it to the build graph (the upstream tree has no
# CMakeLists.txt). We then drive its autotools build out-of-tree.
FetchContent_GetProperties(masstree)
if(NOT masstree_POPULATED)
  FetchContent_Populate(masstree)
endif()

set(_masstree_archive "${masstree_SOURCE_DIR}/libkohler_masstree_json.a")
set(_masstree_config_h "${masstree_SOURCE_DIR}/config.h")

# Mirror the historical bootstrap.sh object set verbatim — these are the
# exact 11 translation units the old script packed into the archive.
set(_masstree_objs
  json.o string.o straccum.o str.o msgpack.o clp.o
  kvrandom.o compiler.o memdebug.o kvthread.o misc.o)

add_custom_command(
  OUTPUT  "${_masstree_archive}" "${_masstree_config_h}"
  COMMAND ${CMAKE_COMMAND} -E echo "Building masstree (libkohler_masstree_json.a)"
  COMMAND ./bootstrap.sh
  COMMAND ./configure --disable-assertions
  COMMAND make -j CXXFLAGS=-g\ -W\ -Wall\ -O3\ -fPIC
  COMMAND ar cr libkohler_masstree_json.a ${_masstree_objs}
  COMMAND ranlib libkohler_masstree_json.a
  WORKING_DIRECTORY "${masstree_SOURCE_DIR}"
  COMMENT "masstree: bootstrap + configure + make + ar"
  VERBATIM
)
add_custom_target(masstree_build DEPENDS "${_masstree_archive}" "${_masstree_config_h}")

# Expose masstree as an INTERFACE library (rather than STATIC IMPORTED) so
# we can use add_dependencies() to wire the upstream-build custom target
# into every consumer — IMPORTED targets don't propagate build-order deps
# reliably across all CMake versions we care about.
add_library(ccbench_masstree INTERFACE)
target_include_directories(ccbench_masstree INTERFACE "${masstree_SOURCE_DIR}")
target_link_libraries(ccbench_masstree INTERFACE "${_masstree_archive}")
add_dependencies(ccbench_masstree masstree_build)
add_library(ccbench::masstree ALIAS ccbench_masstree)

# --- mimalloc --------------------------------------------------------------
#
# mimalloc is a regular CMake project; FetchContent_MakeAvailable drops it
# straight into the build graph and gives us the upstream
# `mimalloc-static` target. We alias it so the rest of the tree keeps
# linking against ccbench::mimalloc unchanged.
#
# Note: we keep the same commit (v2.3.2) the old submodule was pinned to —
# bumping the allocator version is out of scope for issue #98.

set(MI_BUILD_STATIC ON  CACHE BOOL "" FORCE)
set(MI_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(MI_BUILD_OBJECT OFF CACHE BOOL "" FORCE)
set(MI_BUILD_TESTS  OFF CACHE BOOL "" FORCE)
set(MI_OVERRIDE     OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  mimalloc
  GIT_REPOSITORY "${CCBENCH_MIMALLOC_REPO}"
  GIT_TAG        "${CCBENCH_MIMALLOC_TAG}"
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(mimalloc)

# Upstream exports `mimalloc-static`; expose it under ccbench:: so callers
# don't have to know which upstream name to use.
add_library(ccbench::mimalloc ALIAS mimalloc-static)

# --- googletest ------------------------------------------------------------
#
# Standard FetchContent + MakeAvailable. The googletest CMake project
# exports `GTest::gtest` / `GTest::gtest_main` targets that the test
# subdirectories (currently only cc/ss2pl/test) link against directly.

set(BUILD_GMOCK    OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST  OFF CACHE BOOL "" FORCE)
# Match MSVC's runtime to the rest of the build — no-op on Linux but
# keeps the project portable if anyone ever builds tests on Windows.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY "${CCBENCH_GOOGLETEST_REPO}"
  GIT_TAG        "${CCBENCH_GOOGLETEST_TAG}"
  GIT_SHALLOW    FALSE  # SHA pin
)
FetchContent_MakeAvailable(googletest)
