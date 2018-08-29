#ifndef COMMON_HPP
#define COMMON_HPP

#include <atomic>
#include "procedure.hpp"
#include "tuple.hpp"


#ifdef GLOBAL_VALUE_DEFINE
	#define GLOBAL
GLOBAL std::atomic<int> Running(0);
GLOBAL std::atomic<int> Ending(0);
GLOBAL std::atomic<bool> Finish(false);

#else
	#define GLOBAL extern
GLOBAL std::atomic<int> Running;
GLOBAL std::atomic<int> Ending;
GLOBAL std::atomic<bool> Finish;

#endif

//run-time args
GLOBAL int TUPLE_NUM;
GLOBAL int MAX_OPE;
GLOBAL int THREAD_NUM;
GLOBAL int PRO_NUM;
GLOBAL float READ_RATIO;
GLOBAL uint64_t CLOCK_PER_US;
GLOBAL int EXTIME;

GLOBAL uint64_t *AbortCount;
GLOBAL uint64_t *FinishTransactions;

GLOBAL uint64_t *Start;
GLOBAL uint64_t *Stop;
GLOBAL uint64_t Bgn;
GLOBAL uint64_t End;

GLOBAL Procedure **Pro;
GLOBAL Tuple *Table;

#endif	//	COMMON_HPP
