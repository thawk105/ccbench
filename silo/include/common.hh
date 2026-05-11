#pragma once

#include <pthread.h>
#include <atomic>
#include <iostream>
#include <queue>

#include "tuple.hh"

#include "../../include/cache_line_size.hh"
#include "../../include/int64byte.hh"
#include "../../include/masstree_wrapper.hh"

#include "gflags/gflags.h"
#include "glog/logging.h"

#ifdef GLOBAL_VALUE_DEFINE
#define GLOBAL
alignas(CACHE_LINE_SIZE) GLOBAL uint64_t_64byte GlobalEpoch(1);
#if MASSTREE_USE
alignas(CACHE_LINE_SIZE) GLOBAL MasstreeWrapper<Tuple> MT;
#endif
#else
#define GLOBAL extern
alignas(CACHE_LINE_SIZE) GLOBAL uint64_t_64byte GlobalEpoch;
#if MASSTREE_USE
alignas(CACHE_LINE_SIZE) GLOBAL MasstreeWrapper<Tuple> MT;
#endif
#endif

#ifdef GLOBAL_VALUE_DEFINE
DEFINE_uint64(clocks_per_us, 2100,
              "CPU_MHz. Use this info for measuring time.");
DEFINE_uint64(epoch_time, 40, "Epoch interval[msec].");
DEFINE_uint64(extime, 3, "Execution time[sec].");
DEFINE_uint64(thread_num, 10, "Number of batch regular threads.");
DEFINE_bool(ycsb, true,
            "True uses zipf_skew, false uses faster random generator.");
DEFINE_uint64(ronly_ratio, 0, "ratio of read-only online transaction.");
DEFINE_uint64(batch_th_num, 0, "Number of batch worker threads.");
DEFINE_uint64(batch_ratio, 0, "ratio of batch transaction.");
DEFINE_uint64(batch_max_ope, 1000,
              "Total number of operations per single batch transaction.");
DEFINE_uint64(batch_rratio, 100, "read ratio of single batch transaction.");
DEFINE_uint64(batch_tuples, 0, "Number of update-only records for batch transaction.");
DEFINE_bool(batch_simple_rr, false, "No one touches update-only records of batch transaction.");
#else
DECLARE_uint64(clocks_per_us);
DECLARE_uint64(epoch_time);
DECLARE_uint64(extime);
DECLARE_uint64(max_ope);
DECLARE_bool(rmw);
DECLARE_uint64(rratio);
DECLARE_uint64(thread_num);
DECLARE_uint64(tuple_num);
DECLARE_bool(ycsb);
DECLARE_double(zipf_skew);
DECLARE_uint64(ronly_ratio);
DECLARE_uint64(batch_th_num);
DECLARE_uint64(batch_ratio);
DECLARE_uint64(batch_max_ope);
DECLARE_uint64(batch_rratio);
DECLARE_uint64(batch_tuples);
DECLARE_bool(batch_simple_rr);
#endif

alignas(CACHE_LINE_SIZE) GLOBAL uint32_t TotalThreadNum;
alignas(CACHE_LINE_SIZE) GLOBAL uint32_t ReclamationEpoch;
alignas(CACHE_LINE_SIZE) GLOBAL uint64_t_64byte *ThLocalEpoch;
alignas(CACHE_LINE_SIZE) GLOBAL uint64_t_64byte *CTIDW;
