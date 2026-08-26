if(CCBENCH_SS2PL_DLR STREQUAL "0")
  set(SS2PL_DLR_MARKER DLR0)
elseif(CCBENCH_SS2PL_DLR STREQUAL "1")
  set(SS2PL_DLR_MARKER DLR1)
else()
  message(FATAL_ERROR "CCBENCH_SS2PL_DLR must be 0 or 1")
endif()

ccbench_add_protocol(ss2pl
  SOURCES   transaction.cc util.cc
  WORKLOADS ycsb bomb tpcc
  OPTIONS
    ${SS2PL_DLR_MARKER}
    KEY_SORT=${CCBENCH_KEY_SORT}
)
