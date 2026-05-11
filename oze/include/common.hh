#pragma once

#include <pthread.h>
#include <atomic>
#include <iostream>
#include <queue>

#include "../../include/cache_line_size.hh"
#include "../../include/int64byte.hh"
#include "../../include/masstree_wrapper.hh"
#include "../../include/lock.hh"

#include "cc_mode.hh"
#include "debug.hh"
#include "scan.hh"
#include "thread_management.hh"
#include "tuple.hh"
#include "version.hh"

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
DEFINE_uint64(clocks_per_us, 2100, "CPU_MHz. Use this info for measuring time.");
DEFINE_uint64(extime, 3, "Execution time[sec].");
DEFINE_uint64(epoch_time, 40, "Epoch interval[msec].");
DEFINE_uint64(gc_inter_us, 10, "GC interval[us].");
DEFINE_uint64(group_commit, 0, "Group commit number of transactions.");
DEFINE_uint64(group_commit_timeout_us, 2, "Timeout used for deadlock resolution when performing group commit[us].");
DEFINE_uint64(io_time_ns, 5, "Delay inserted instead of IO.");
DEFINE_uint64(pre_reserve_version, 10000, "Pre-allocating memory for the version.");
DEFINE_bool(p_wal, false, "Parallel write-ahead logging.");
DEFINE_bool(s_wal, false, "Normal write-ahead logging.");
DEFINE_uint64(thread_num, 10, "Number of regular worker threads.");
DEFINE_bool(forwarding, true, "Enable order forwarding.");
DEFINE_uint64(validation_threshold, 100, "Threshold of pages for parallel validation.");
DEFINE_uint64(validation_th_num, 1, "Number of validation threads.");
DEFINE_uint64(cc_mode, DEFAULT_CC_MODE, "Initial concurrency control mode.");
#else
DECLARE_uint64(clocks_per_us);
DECLARE_uint64(extime);
DECLARE_uint64(epoch_time);
DECLARE_uint64(gc_inter_us);
DECLARE_uint64(group_commit);
DECLARE_uint64(group_commit_timeout_us);
DECLARE_uint64(io_time_ns);
DECLARE_uint64(pre_reserve_version);
DECLARE_bool(p_wal);
DECLARE_bool(s_wal);
DECLARE_uint64(thread_num);
DECLARE_bool(forwarding);
DECLARE_uint64(validation_threshold);
DECLARE_uint64(validation_th_num);
DECLARE_uint64(cc_mode);
#endif

GLOBAL uint64_t TotalThreadNum;

alignas(CACHE_LINE_SIZE) GLOBAL Tuple *Table;
alignas(CACHE_LINE_SIZE) GLOBAL ThreadManagementEntry **ThManagementTable;

#define SCAN_HISTORY_INDEX(storage, epoch) 2 * (uint32_t)storage + epoch % 2
alignas(CACHE_LINE_SIZE) GLOBAL atomic<ScanEntry*> *ScanHistory;
alignas(CACHE_LINE_SIZE) GLOBAL atomic<uint64_t> *ScanRange;
