# Build-time tunables for ccbench.
#
# Every value below is a CACHE entry, so it can be overridden once on the
# CMake command line, e.g.
#   cmake -S . -B build -DCCBENCH_KEY_SIZE=16 -DCCBENCH_BACK_OFF=0
# without editing this file.
#
# Replaces the ~180 directory-scoped `add_definitions(...)` calls
# previously copy-pasted into every protocol's CMakeLists.txt.
# `ccbench_add_protocol(...)` (cmake/ProtocolHelpers.cmake) maps each
# CCBENCH_FOO into a target-private `-DFOO=<value>`.

# ----- universal (every protocol uses these) -----
set(CCBENCH_ADD_ANALYSIS  0 CACHE STRING "extra per-tx analysis counters")
set(CCBENCH_BACK_OFF      1 CACHE STRING "exponential backoff on abort")
set(CCBENCH_KEY_SIZE      8 CACHE STRING "key size in bytes (YCSB)")
set(CCBENCH_MASSTREE_USE  1 CACHE STRING "use Masstree as the index")
set(CCBENCH_VAL_SIZE      4 CACHE STRING "value size in bytes (YCSB)")

# ----- multi-protocol (used by 2+ protocols) -----
set(CCBENCH_KEY_SORT                       0 CACHE STRING "")
set(CCBENCH_NO_WAIT_LOCKING_IN_VALIDATION  1 CACHE STRING "")
set(CCBENCH_NO_WAIT_OF_TICTOC              0 CACHE STRING "")
set(CCBENCH_PARTITION_TABLE                0 CACHE STRING "")
set(CCBENCH_SLEEP_READ_PHASE               0 CACHE STRING "")
set(CCBENCH_INLINE_VERSION_PROMOTION       1 CACHE STRING "")
set(CCBENCH_REUSE_VERSION                  1 CACHE STRING "")
set(CCBENCH_SINGLE_EXEC                    0 CACHE STRING "")
set(CCBENCH_WRITE_LATEST_ONLY              0 CACHE STRING "")
# These two are intentionally unset by default (the original code used
# remove_definitions when not provided; an empty cache entry maps the
# same way in `ccbench_add_protocol`).
set(CCBENCH_INSERT_READ_DELAY_MS  "" CACHE STRING "ms; empty = unset")
set(CCBENCH_INSERT_BATCH_DELAY_MS "" CACHE STRING "ms; empty = unset")

# ----- protocol-specific defaults (live here for centralized override) -----
set(CCBENCH_DEBUG_MSG                     0 CACHE STRING "oze")
set(CCBENCH_MERGE_ON_READ                 0 CACHE STRING "oze")
set(CCBENCH_PREEMPTIVE_ABORTS             1 CACHE STRING "tictoc")
set(CCBENCH_TIMESTAMP_HISTORY             1 CACHE STRING "tictoc")
set(CCBENCH_PROCEDURE_SORT                0 CACHE STRING "silo")
set(CCBENCH_WAL                           0 CACHE STRING "silo")
set(CCBENCH_TEMPERATURE_RESET_OPT         1 CACHE STRING "mocc")
set(CCBENCH_WORKER1_INSERT_DELAY_RPHASE   0 CACHE STRING "cicada")
set(CCBENCH_SS2PL_DLR                     1 CACHE STRING "ss2pl: 0=timeout, 1=no-wait")

# Cicada and Oze share many flags but disagree on this one's default.
# Keep two separate cache entries; protocols pick the one they want.
set(CCBENCH_INLINE_VERSION_OPT_CICADA     0 CACHE STRING "cicada")
set(CCBENCH_INLINE_VERSION_OPT_OZE        1 CACHE STRING "oze")

# ----- helpers --------------------------------------------------------------

# Emit the universal flag list as `KEY=VALUE` items (suitable for
# target_compile_definitions). One item per call site.
function(ccbench_universal_definitions out_var)
  set(${out_var}
    ADD_ANALYSIS=${CCBENCH_ADD_ANALYSIS}
    BACK_OFF=${CCBENCH_BACK_OFF}
    KEY_SIZE=${CCBENCH_KEY_SIZE}
    MASSTREE_USE=${CCBENCH_MASSTREE_USE}
    VAL_SIZE=${CCBENCH_VAL_SIZE}
    PARENT_SCOPE)
endfunction()

# Convenience: turn `<NAME>=<VALUE>` into `-D<NAME>=<VALUE>` and append to
# `out_var`. If VALUE is empty, the entry is dropped (mimics the previous
# `remove_definitions(-DFLAG)` behavior).
function(ccbench_normalize_options out_var)
  set(_acc "")
  foreach(item IN LISTS ARGN)
    if(item MATCHES "^([^=]+)=(.*)$")
      set(_k "${CMAKE_MATCH_1}")
      set(_v "${CMAKE_MATCH_2}")
      if(NOT _v STREQUAL "")
        list(APPEND _acc "${_k}=${_v}")
      endif()
    else()
      list(APPEND _acc "${item}")
    endif()
  endforeach()
  set(${out_var} "${_acc}" PARENT_SCOPE)
endfunction()
