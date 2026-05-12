ccbench_add_protocol(mvto
  SOURCES   transaction.cc util.cc result.cc
  WORKLOADS bomb tpcc
  OPTIONS
    INSERT_READ_DELAY_MS=${CCBENCH_INSERT_READ_DELAY_MS}
    INSERT_BATCH_DELAY_MS=${CCBENCH_INSERT_BATCH_DELAY_MS}
)
