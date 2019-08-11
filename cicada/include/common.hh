#pragma once

#include <pthread.h>
#include <atomic>
#include <iostream>
#include <queue>

#include "../../include/cache_line_size.hh"
#include "../../include/int64byte.hh"
#include "../../include/masstree_wrapper.hh"
#include "lock.hh"
#include "tuple.hh"
#include "version.hh"

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

GLOBAL size_t TUPLE_NUM;
GLOBAL size_t MAX_OPE;
GLOBAL size_t THREAD_NUM;
GLOBAL size_t RRATIO;
GLOBAL bool RMW;
GLOBAL double ZIPF_SKEW;
GLOBAL bool YCSB;
GLOBAL bool P_WAL;
GLOBAL bool S_WAL;
GLOBAL size_t GROUP_COMMIT;
GLOBAL size_t CLOCKS_PER_US;            // US = micro(µ) seconds
GLOBAL size_t IO_TIME_NS;               // nano second
GLOBAL size_t GROUP_COMMIT_TIMEOUT_US;  // micro seconds
GLOBAL size_t GC_INTER_US;              // garbage collection interval
GLOBAL size_t PRE_RESERVE_VERSION;
GLOBAL size_t EXTIME;

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
