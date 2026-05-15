ccbench_add_protocol(si
  SOURCES   transaction.cc util.cc garbage_collection.cc
  WORKLOADS ycsb tpcc bomb sbomb
  OPTIONS
    KEY_SORT=${CCBENCH_KEY_SORT}
    INSERT_READ_DELAY_MS=${CCBENCH_INSERT_READ_DELAY_MS}
    INSERT_BATCH_DELAY_MS=${CCBENCH_INSERT_BATCH_DELAY_MS}
)
