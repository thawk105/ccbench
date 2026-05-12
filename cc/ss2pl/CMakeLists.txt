ccbench_add_protocol(ss2pl
  SOURCES   transaction.cc util.cc result.cc
  WORKLOADS bomb tpcc
  OPTIONS
    DLR1
    KEY_SORT=${CCBENCH_KEY_SORT}
)
