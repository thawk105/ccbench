#pragma once

#include <atomic>

#include "../../include/cache_line_size.hpp"
#include "../../include/int64byte.hpp"

#include "procedure.hpp"
#include "tuple.hpp"

#ifdef GLOBAL_VALUE_DEFINE
  #define GLOBAL
GLOBAL std::atomic<size_t> Running(0);

#else
  #define GLOBAL extern
GLOBAL std::atomic<size_t> Running;

#endif

//run-time args
GLOBAL size_t TUPLE_NUM;
GLOBAL size_t MAX_OPE;
GLOBAL size_t THREAD_NUM;
GLOBAL size_t RRATIO;
GLOBAL bool RMW;
GLOBAL double ZIPF_SKEW;
GLOBAL bool YCSB;
GLOBAL size_t CLOCKS_PER_US;
GLOBAL size_t EXTIME;

alignas(CACHE_LINE_SIZE) GLOBAL Tuple *Table;
