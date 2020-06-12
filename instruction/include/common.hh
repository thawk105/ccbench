#pragma once

#include <atomic>
#include <mutex>
#include <vector>

#include "../../include/int64byte.hh"

#ifdef GLOBAL_VALUE_DEFINE
#define GLOBAL
GLOBAL std::atomic<unsigned int> Running(0);
GLOBAL std::atomic<uint64_t> Counter(0);
GLOBAL std::atomic<bool> Finish(false);

#else
#define GLOBAL extern
GLOBAL std::atomic<unsigned int> Running;
GLOBAL std::atomic <uint64_t> Counter;
GLOBAL std::atomic<bool> Finish;

#endif

//run-time args
GLOBAL unsigned int THREAD_NUM;
GLOBAL uint64_t CLOCK_PER_US;
GLOBAL unsigned int EXTIME;

GLOBAL uint64_t_64byte *CounterIncrements;

GLOBAL uint64_t Bgn;
GLOBAL uint64_t End;

