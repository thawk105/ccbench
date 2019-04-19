#pragma once
#include <pthread.h>
#include <iostream>
#include <atomic>
#include <queue>

#include "lock.hpp"
#include "procedure.hpp"
#include "tuple.hpp"

#include "../../include/cache_line_size.hpp"
#include "../../include/int64byte.hpp"

#ifdef GLOBAL_VALUE_DEFINE
  #define GLOBAL

GLOBAL std::atomic<size_t> Running(0);
alignas(CACHE_LINE_SIZE) GLOBAL uint64_t_64byte GlobalEpoch(1);

#else
  #define GLOBAL extern

GLOBAL std::atomic<size_t> Running;
alignas(CACHE_LINE_SIZE) GLOBAL uint64_t_64byte GlobalEpoch;

#endif

GLOBAL size_t TUPLE_NUM;
GLOBAL size_t MAX_OPE;
GLOBAL size_t THREAD_NUM;
GLOBAL size_t RRATIO;
GLOBAL bool RMW;
GLOBAL double ZIPF_SKEW;
GLOBAL bool YCSB;
GLOBAL size_t CLOCKS_PER_US;
GLOBAL size_t EPOCH_TIME;
GLOBAL size_t EXTIME;

alignas(CACHE_LINE_SIZE) GLOBAL uint64_t_64byte *ThLocalEpoch;
alignas(CACHE_LINE_SIZE) GLOBAL uint64_t_64byte *CTIDW;

alignas(CACHE_LINE_SIZE) GLOBAL Tuple *Table;
