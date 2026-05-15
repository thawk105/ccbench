include(CompileOptions)

# ccbench_add_microbench(<name>
#   SOURCES         <microbench .cc files>     # entry point + any extra .cc
#   [COMPILE_OPTIONS <flags...>]               # extra target-private flags
# )
#
# Builds an executable `<name>.exe` from SOURCES, links it against
# ccbench_common (which carries common/util.cc, the include/ search path
# and the Threads/Boost/gflags/glog dependencies), and applies the same
# -Wall -Wextra -Werror via set_compile_options() that every protocol
# binary gets.
#
# COMPILE_OPTIONS is for the rare bench that needs an explicit ISA
# extension flag (e.g. membench uses the _mm_clwb intrinsic and so needs
# -mclwb). We pass the specific flag instead of the old Makefile's
# -march=native, which is not reproducible across the machines that
# build / run CI.
#
# Microbenchmarks measure the *cost* of a single instruction / operation
# (atomic RMW throughput, RNG clocks, zipf distribution, rdtsc latency,
# memory bandwidth). They are opt-in: the top-level CMakeLists only does
# add_subdirectory(microbench) when -DCCBENCH_BUILD_MICROBENCH=ON, so the
# default build (and CI) is unaffected.
function(ccbench_add_microbench name)
  set(opts "")
  set(one_value "")
  set(multi_value "SOURCES;COMPILE_OPTIONS")
  cmake_parse_arguments(M "${opts}" "${one_value}" "${multi_value}" ${ARGN})

  if(NOT M_SOURCES)
    message(FATAL_ERROR "ccbench_add_microbench(${name}): SOURCES is required")
  endif()

  set(target "${name}.exe")
  add_executable(${target} ${M_SOURCES})

  target_link_libraries(${target} PRIVATE ccbench_common)

  # cpu.hh / util.hh gate their Linux-only paths behind `#ifdef Linux`.
  # The old hand-written Makefile supplied this via `-D$(shell uname)`;
  # keep the same contract here instead of editing the shared headers.
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(${target} PRIVATE Linux)
  endif()

  if(M_COMPILE_OPTIONS)
    target_compile_options(${target} PRIVATE ${M_COMPILE_OPTIONS})
  endif()

  set_compile_options(${target})
endfunction()
