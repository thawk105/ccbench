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
alignas(64) GLOBAL uint64_t_64byte GlobalEpoch(1);
GLOBAL std::atomic<bool> Finish(false);
GLOBAL std::atomic<uint64_t> FinishTransactions(0);
GLOBAL std::atomic<uint64_t> AbortCounts(0);

#else
	#define GLOBAL extern

GLOBAL std::atomic<unsigned int> Running;
GLOBAL uint64_t_64byte GlobalEpoch;
GLOBAL std::atomic<bool> Finish;
GLOBAL std::atomic<uint64_t> FinishTransactions;
GLOBAL std::atomic<uint64_t> AbortCounts;

#endif

GLOBAL unsigned int TUPLE_NUM;
GLOBAL unsigned int MAX_OPE;
GLOBAL unsigned int THREAD_NUM;
GLOBAL unsigned int WORKLOAD;
GLOBAL uint64_t CLOCK_PER_US;
GLOBAL uint64_t EPOCH_TIME;
GLOBAL unsigned int EXTIME;

GLOBAL uint64_t_64byte *ThLocalEpoch;

GLOBAL uint64_t Bgn;
GLOBAL uint64_t End;

GLOBAL Procedure **Pro;

GLOBAL Tuple *Table;
