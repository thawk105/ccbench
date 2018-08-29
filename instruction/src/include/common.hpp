#ifndef COMMON_HPP
#define COMMON_HPP

#include "int64byte.hpp"

#include <mutex>
#include <vector>

#ifdef GLOBAL_VALUE_DEFINE
	#define GLOBAL
GLOBAL std::atomic<int> Running(0);
GLOBAL std::atomic<uint64_t> Counter(0);
GLOBAL std::atomic<bool> Finish(false);

#else
	#define GLOBAL extern
GLOBAL std::atomic<int> Running;
GLOBAL std::atomic<uint64_t> Lsn;
GLOBAL std::atomic<bool> Finish;

#endif

//run-time args
GLOBAL int THREAD_NUM;
GLOBAL uint64_t CLOCK_PER_US;
GLOBAL int EXTIME;

GLOBAL uint64_t_64byte *CounterIncrements;

GLOBAL uint64_t Bgn;
GLOBAL uint64_t End;

#endif	//	COMMON_HPP
