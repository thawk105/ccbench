#pragma once

#include <mutex>
#include <vector>

#include "../../include/cache_line_size.hpp"
#include "../../include/int64byte.hpp"

#include "lock.hpp"
#include "procedure.hpp"
#include "transaction.hpp"
#include "tuple.hpp"

#ifdef GLOBAL_VALUE_DEFINE
  #define GLOBAL
GLOBAL std::atomic<bool> Finish(false);
GLOBAL std::atomic<uint64_t> Lsn(0);
GLOBAL std::atomic<unsigned int> Running(0);

#else
  #define GLOBAL extern
GLOBAL std::atomic<bool> Finish;
GLOBAL std::atomic<uint64_t> Lsn;
GLOBAL std::atomic<unsigned int> Running;

#endif

// run-time args
GLOBAL unsigned int TUPLE_NUM;
GLOBAL unsigned int MAX_OPE;
GLOBAL unsigned int THREAD_NUM;
GLOBAL unsigned int RRATIO;
GLOBAL bool RMW;
GLOBAL double ZIPF_SKEW;
GLOBAL bool YCSB;
GLOBAL uint64_t CLOCK_PER_US;
GLOBAL uint64_t GC_INTER_US; // garbage collection interval
GLOBAL uint64_t EXTIME;
// -----

GLOBAL Tuple *Table;
alignas(CACHE_LINE_SIZE) GLOBAL TransactionTable **TMT;  // Transaction Mapping Table

GLOBAL std::mutex SsnLock;
