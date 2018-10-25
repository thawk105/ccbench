#pragma once

#include "int64byte.hpp"
#include "lock.hpp"
#include "procedure.hpp"
#include "tuple.hpp"
#include <pthread.h>
#include <iostream>
#include <atomic>
#include <queue>

#ifdef GLOBAL_VALUE_DEFINE
	#define GLOBAL

GLOBAL std::atomic<unsigned int> Running(0);
GLOBAL std::atomic<bool> Finish(false);
GLOBAL std::atomic<uint64_t> FinishTransactions(0);
GLOBAL std::atomic<uint64_t> AbortCounts(0);
GLOBAL std::atomic<uint64_t> Rtsudctr(0);
GLOBAL std::atomic<uint64_t> Rts_non_udctr(0);

#else
	#define GLOBAL extern

GLOBAL std::atomic<unsigned int> Running;
GLOBAL std::atomic<bool> Finish;
GLOBAL std::atomic<uint64_t> FinishTransactions;
GLOBAL std::atomic<uint64_t> AbortCounts;
GLOBAL std::atomic<uint64_t> Rtsudctr;
GLOBAL std::atomic<uint64_t> Rts_non_udctr;

#endif

GLOBAL unsigned int TUPLE_NUM;
GLOBAL unsigned int MAX_OPE;
GLOBAL unsigned int THREAD_NUM;
GLOBAL unsigned int WORKLOAD;
GLOBAL uint64_t CLOCK_PER_US;
GLOBAL unsigned int EXTIME;

GLOBAL uint64_t Bgn;
GLOBAL uint64_t End;

GLOBAL Procedure **Pro;

GLOBAL Tuple *Table;
