#pragma once

#include <pthread.h>

#include <atomic>
#include <iostream>
#include <queue>

#include "../../include/cache_line_size.hpp"
#include "../../include/int64byte.hpp"

#include "lock.hpp"
#include "procedure.hpp"
#include "tuple.hpp"

#ifdef GLOBAL_VALUE_DEFINE
  #define GLOBAL

GLOBAL std::atomic<size_t> Running(0);

#else
  #define GLOBAL extern

GLOBAL std::atomic<size_t> Running;

#endif

GLOBAL size_t TUPLE_NUM;
GLOBAL size_t MAX_OPE;
GLOBAL size_t THREAD_NUM;
GLOBAL size_t RRATIO;
GLOBAL bool RMW;
GLOBAL double ZIPF_SKEW;
GLOBAL bool YCSB;
GLOBAL size_t CLOCKS_PER_US;
GLOBAL size_t EXTIME;

alignas(64) GLOBAL Tuple *Table;
