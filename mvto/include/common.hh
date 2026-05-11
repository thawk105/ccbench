#pragma once

#include <pthread.h>
#include <atomic>
#include <iostream>
#include <queue>

#include "../../include/cache_line_size.hh"
#include "../../include/int64byte.hh"
#include "../../include/lock.hh"
#include "../../include/masstree_wrapper.hh"
#include "tuple.hh"
#include "version.hh"

#include "gflags/gflags.h"
#include "glog/logging.h"

#ifdef GLOBAL_VALUE_DEFINE
#define GLOBAL
alignas(CACHE_LINE_SIZE) GLOBAL std::atomic<uint64_t> MinRts(0);
alignas(CACHE_LINE_SIZE) GLOBAL std::atomic<uint64_t> MinWts(0);
alignas(
    CACHE_LINE_SIZE) GLOBAL std::atomic<unsigned int> FirstAllocateTimestamp(0);
#if MASSTREE_USE
alignas(CACHE_LINE_SIZE) GLOBAL MasstreeWrapper<Tuple> MT;
#endif
#else
#define GLOBAL extern
alignas(CACHE_LINE_SIZE) GLOBAL std::atomic<uint64_t> MinRts;
alignas(CACHE_LINE_SIZE) GLOBAL std::atomic<uint64_t> MinWts;
alignas(
CACHE_LINE_SIZE) GLOBAL std::atomic<unsigned int> FirstAllocateTimestamp;
#if MASSTREE_USE
alignas(CACHE_LINE_SIZE) GLOBAL MasstreeWrapper<Tuple> MT;
#endif
#endif

#ifdef GLOBAL_VALUE_DEFINE
DEFINE_uint64(clocks_per_us, 2100, "CPU_MHz. Use this info for measuring time.");
DEFINE_uint64(extime, 3, "Execution time[sec].");
DEFINE_uint64(gc_inter_us, 10, "GC interval[us].");
DEFINE_uint64(group_commit, 0, "Group commit number of transactions.");
DEFINE_uint64(group_commit_timeout_us, 2, "Timeout used for deadlock resolution when performing group commit[us].");
DEFINE_uint64(io_time_ns, 5, "Delay inserted instead of IO.");
DEFINE_uint64(thread_num, 10, "Total number of worker threads.");
DEFINE_bool(preserve_write, false, "Install pending version in write operation");
#else
DECLARE_uint64(clocks_per_us);
DECLARE_uint64(extime);
DECLARE_uint64(gc_inter_us);
DECLARE_uint64(io_time_ns);
DECLARE_uint64(thread_num);
DECLARE_bool(preserve_write);
#endif

GLOBAL uint64_t TotalThreadNum;

alignas(CACHE_LINE_SIZE) GLOBAL uint64_t_64byte *ThreadWtsArray;
alignas(CACHE_LINE_SIZE) GLOBAL uint64_t_64byte *ThreadRtsArray;
alignas(CACHE_LINE_SIZE) GLOBAL uint64_t_64byte
        *ThreadRtsArrayForGroup;  // グループコミットをする時，これが必要である．

alignas(CACHE_LINE_SIZE) GLOBAL uint64_t_64byte *GROUP_COMMIT_INDEX;
alignas(CACHE_LINE_SIZE) GLOBAL uint64_t_64byte
        *GROUP_COMMIT_COUNTER;  // s-walの時は[0]のみ使用。全スレッドで共有。

alignas(
CACHE_LINE_SIZE) GLOBAL Version ***PLogSet;  // [thID][index] pointer array
alignas(CACHE_LINE_SIZE) GLOBAL Version **SLogSet;  // [index] pointer array
GLOBAL RWLock SwalLock;
GLOBAL RWLock CtrLock;

alignas(CACHE_LINE_SIZE) GLOBAL uint64_t_64byte *GCFlag;
alignas(CACHE_LINE_SIZE) GLOBAL uint64_t_64byte *GCExecuteFlag;

alignas(CACHE_LINE_SIZE) GLOBAL Tuple *Table;
alignas(CACHE_LINE_SIZE) GLOBAL uint64_t InitialWts;

#define SPIN_WAIT_TIMEOUT_US 2
