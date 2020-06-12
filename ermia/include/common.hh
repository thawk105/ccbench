#pragma once

#include <mutex>
#include <vector>

#include "lock.hh"
#include "transaction_table.hh"
#include "tuple.hh"

#include "../../include/cache_line_size.hh"
#include "../../include/int64byte.hh"
#include "../../include/masstree_wrapper.hh"

#include "gflags/gflags.h"
#include "glog/logging.h"

#ifdef GLOBAL_VALUE_DEFINE
#define GLOBAL
GLOBAL std::atomic<uint64_t> Lsn(0);
#if MASSTREE_USE
alignas(CACHE_LINE_SIZE) GLOBAL MasstreeWrapper<Tuple> MT;
#endif
#else
#define GLOBAL extern
GLOBAL std::atomic<uint64_t> Lsn;
#if MASSTREE_USE
alignas(CACHE_LINE_SIZE) GLOBAL MasstreeWrapper<Tuple> MT;
#endif
#endif

#ifdef GLOBAL_VALUE_DEFINE
DEFINE_uint64(clocks_per_us, 2100, "CPU_MHz. Use this info for measuring time.");
DEFINE_uint64(extime, 3, "Execution time[sec].");
DEFINE_uint64(gc_inter_us, 10, "GC interval[us].");
DEFINE_uint64(max_ope, 10,
              "Total number of operations per single transaction.");
DEFINE_uint64(pre_reserve_tmt_element, 100, "Pre-allocating memory for the transaction mapping table elements.");
DEFINE_uint64(pre_reserve_version, 10000, "Pre-allocating memory for the version.");
DEFINE_bool(rmw, false,
            "True means read modify write, false means blind write.");
DEFINE_uint64(rratio, 50, "read ratio of single transaction.");
DEFINE_uint64(thread_num, 10, "Total number of worker threads.");
DEFINE_uint64(tuple_num, 1000000, "Total number of records.");
DEFINE_bool(ycsb, true,
            "True uses zipf_skew, false uses faster random generator.");
DEFINE_double(zipf_skew, 0, "zipf skew. 0 ~ 0.999...");
#else
DECLARE_uint64(clocks_per_us);
DECLARE_uint64(extime);
DECLARE_uint64(gc_inter_us);
DECLARE_uint64(max_ope);
DECLARE_uint64(pre_reserve_tmt_element);
DECLARE_uint64(pre_reserve_version);
DECLARE_bool(rmw);
DECLARE_uint64(rratio);
DECLARE_uint64(thread_num);
DECLARE_uint64(tuple_num);
DECLARE_bool(ycsb);
DECLARE_double(zipf_skew);
#endif


alignas(CACHE_LINE_SIZE) GLOBAL Tuple *Table;
alignas(CACHE_LINE_SIZE) GLOBAL
TransactionTable **TMT;  // Transaction Mapping Table

GLOBAL std::mutex SsnLock;
