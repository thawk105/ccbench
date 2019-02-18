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

GLOBAL std::atomic<unsigned int> Running(0);
alignas(CACHE_LINE_SIZE) GLOBAL uint64_t_64byte GlobalEpoch(1);

#else
  #define GLOBAL extern

GLOBAL std::atomic<unsigned int> Running;
alignas(CACHE_LINE_SIZE) GLOBAL uint64_t_64byte GlobalEpoch;

#endif

GLOBAL unsigned int TUPLE_NUM;
GLOBAL unsigned int MAX_OPE;
GLOBAL unsigned int THREAD_NUM;
GLOBAL unsigned int RRATIO;
GLOBAL bool RMW;
GLOBAL double ZIPF_SKEW;
GLOBAL bool YCSB;
GLOBAL uint64_t CLOCK_PER_US;
GLOBAL uint64_t EPOCH_TIME;
GLOBAL unsigned int EXTIME;

GLOBAL uint64_t_64byte *ThLocalEpoch;
GLOBAL uint64_t_64byte *CTIDW;

GLOBAL Tuple *Table;
