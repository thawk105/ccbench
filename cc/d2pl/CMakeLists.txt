ccbench_add_protocol(d2pl
  SOURCES   transaction.cc util.cc
  WORKLOADS sbomb dbomb
  OPTIONS
    KEY_SORT=${CCBENCH_KEY_SORT}
)
