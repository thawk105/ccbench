#pragma once

#include <pthread.h>
#include <iostream>
#include <atomic>
#include <queue>

#include "lock.hpp"
#include "procedure.hpp"
#include "tuple.hpp"
#include "version.hpp"

#include "../../include/cache_line_size.hpp"
#include "../../include/int64byte.hpp"

#ifdef GLOBAL_VALUE_DEFINE
  #define GLOBAL

GLOBAL std::atomic<unsigned int> Running(0);
GLOBAL std::atomic<bool> Finish(false);
alignas(CACHE_LINE_SIZE) GLOBAL std::atomic<uint64_t> MinRts(0);
alignas(CACHE_LINE_SIZE) GLOBAL std::atomic<uint64_t> MinWts(0);
alignas(CACHE_LINE_SIZE) GLOBAL std::atomic<unsigned int> FirstAllocateTimestamp(0);

#else
  #define GLOBAL extern

GLOBAL std::atomic<unsigned int> Running;
GLOBAL std::atomic<bool> Finish;
alignas(CACHE_LINE_SIZE) GLOBAL std::atomic<uint64_t> MinRts;
alignas(CACHE_LINE_SIZE) GLOBAL std::atomic<uint64_t> MinWts;
alignas(CACHE_LINE_SIZE) GLOBAL std::atomic<unsigned int> FirstAllocateTimestamp;

#endif

GLOBAL unsigned int TUPLE_NUM;
GLOBAL unsigned int MAX_OPE;
GLOBAL unsigned int THREAD_NUM;
GLOBAL unsigned int RRATIO;
GLOBAL bool RMW;
GLOBAL double ZIPF_SKEW;
GLOBAL bool YCSB;
GLOBAL bool P_WAL;
GLOBAL bool S_WAL;
GLOBAL bool ELR;  // early lock release
GLOBAL bool NLR;
GLOBAL unsigned int GROUP_COMMIT;
GLOBAL uint64_t CLOCK_PER_US; // US = micro(µ) seconds 
GLOBAL double IO_TIME_NS; // nano second
GLOBAL uint64_t GROUP_COMMIT_TIMEOUT_US; // micro seconds
GLOBAL uint64_t GC_INTER_US; // garbage collection interval
GLOBAL uint64_t EXTIME;

GLOBAL uint64_t_64byte *ThreadWtsArray;
GLOBAL uint64_t_64byte *ThreadRtsArray;
GLOBAL uint64_t_64byte *ThreadRtsArrayForGroup; // グループコミットをする時，これが必要である．

GLOBAL uint64_t_64byte *GROUP_COMMIT_INDEX;
GLOBAL uint64_t_64byte *GROUP_COMMIT_COUNTER; // s-walの時は[0]のみ使用。全スレッドで共有。

GLOBAL Version ***PLogSet;  // [thID][index] pointer array
GLOBAL Version **SLogSet; // [index] pointer array
GLOBAL RWLock SwalLock;
GLOBAL RWLock CtrLock;

GLOBAL uint64_t_64byte *GCFlag;
GLOBAL uint64_t_64byte *GCExecuteFlag;

GLOBAL Tuple *Table;
GLOBAL uint64_t InitialWts;

#define SPIN_WAIT_TIMEOUT_US 2
