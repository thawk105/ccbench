#pragma once

#include <atomic>

#include "../../include/cache_line_size.hpp"
#include "../../include/int64byte.hpp"
#include "../../include/random.hpp"

#include "procedure.hpp"
#include "tuple.hpp"

#ifdef GLOBAL_VALUE_DEFINE
  #define GLOBAL

GLOBAL std::atomic<unsigned int> Running(0);
alignas(CACHE_LINE_SIZE) GLOBAL uint64_t_64byte GlobalEpoch(1);

#else
  #define GLOBAL extern

GLOBAL std::atomic<unsigned int> Running;
alignas(CACHE_LINE_SIZE) GLOBAL uint64_t_64byte GlobalEpoch;

#endif

alignas(CACHE_LINE_SIZE) GLOBAL uint64_t_64byte *ThLocalEpoch;

// run-time args
GLOBAL unsigned int TUPLE_NUM;
GLOBAL unsigned int MAX_OPE;
GLOBAL unsigned int THREAD_NUM;
GLOBAL unsigned int RRATIO;
GLOBAL bool RMW;
GLOBAL double ZIPF_SKEW;
GLOBAL bool YCSB;
GLOBAL uint64_t CLOCKS_PER_US;
GLOBAL uint64_t EPOCH_TIME;
GLOBAL unsigned int EXTIME;

GLOBAL RWLock CtrLock;

// for logging emulation
alignas(CACHE_LINE_SIZE) GLOBAL uint64_t_64byte *Start;  
alignas(CACHE_LINE_SIZE) GLOBAL uint64_t_64byte *Stop;

alignas(CACHE_LINE_SIZE) GLOBAL Tuple *Table;

// 下記，ifdef で外すと lock.cc のコードから
// 参照出来なくてエラーが起きる．
// lock.hpp, lock.cc の全てを ifdef で分岐させるのは大変な労力なので，
// 行わず，これは RWLOCK モードでも宣言だけしておく．
alignas(CACHE_LINE_SIZE) GLOBAL MQLNode **MQLNodeTable;
// first dimension index number corresponds to the thread number.
// second dimension index number corresponds to the key of records.
// the element mean MQLnode whihch is owned by the thread which has the thread number corresponding to index number.
// for MQL sentinel value
// index 0 mean None.
// index 1 mean Acquired.
// index 2 mean SuccessorLeaving.
