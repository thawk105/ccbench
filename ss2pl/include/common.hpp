#pragma once

#include <atomic>

#include "../../include/int64byte.hpp"

#include "procedure.hpp"
#include "tuple.hpp"

#ifdef GLOBAL_VALUE_DEFINE
  #define GLOBAL
GLOBAL std::atomic<unsigned int> Running(0);

#else
  #define GLOBAL extern
GLOBAL std::atomic<unsigned int> Running;

#endif

//run-time args
GLOBAL unsigned int TUPLE_NUM;
GLOBAL unsigned int MAX_OPE;
GLOBAL unsigned int THREAD_NUM;
GLOBAL unsigned int RRATIO;
GLOBAL bool RMW;
GLOBAL double ZIPF_SKEW;
GLOBAL bool YCSB;
GLOBAL uint64_t CLOCK_PER_US;
GLOBAL unsigned int EXTIME;

GLOBAL Tuple *Table;
