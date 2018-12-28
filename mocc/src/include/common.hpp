#pragma once

#include "../../../include/int64byte.hpp"
#include "procedure.hpp"
#include "random.hpp"
#include "tuple.hpp"
#include <atomic>

#ifdef GLOBAL_VALUE_DEFINE
	#define GLOBAL

GLOBAL std::atomic<unsigned int> Running(0);
alignas(64) GLOBAL uint64_t_64byte GlobalEpoch(1);
GLOBAL std::atomic<bool> Finish(false);

#else
	#define GLOBAL extern

GLOBAL std::atomic<int> Running;
GLOBAL uint64_t_64byte GlobalEpoch;
GLOBAL std::atomic<bool> Finish;

#endif

GLOBAL uint64_t_64byte *ThLocalEpoch;

// run-time args
GLOBAL unsigned int TUPLE_NUM;
GLOBAL unsigned int MAX_OPE;
GLOBAL unsigned int THREAD_NUM;
GLOBAL unsigned int WORKLOAD;
GLOBAL uint64_t CLOCK_PER_US;
GLOBAL uint64_t EPOCH_TIME;
GLOBAL unsigned int EXTIME;

GLOBAL uint64_t *FinishTransactions;
GLOBAL uint64_t *AbortCounts;

GLOBAL RWLock CtrLock;

// for logging emulation
GLOBAL uint64_t_64byte *Start;	
GLOBAL uint64_t_64byte *Stop;

GLOBAL uint64_t Bgn;
GLOBAL uint64_t End;

GLOBAL Tuple *Table;

// 下記，ifdef で外すと lock.cc のコードから
// 参照出来なくてエラーが起きる．
// lock.hpp, lock.cc の全てを ifdef で分岐させるのは大変な労力なので，
// 行わず，これは RWLOCK モードでも宣言だけしておく．
alignas(64) GLOBAL MQLNode **MQLNodeTable;
// first dimension index number corresponds to the thread number.
// second dimension index number corresponds to the key of records.
// the element mean MQLnode whihch is owned by the thread which has the thread number corresponding to index number.
// for MQL sentinel value
// index 0 mean None.
// index 1 mean Acquired.
// index 2 mean SuccessorLeaving.
