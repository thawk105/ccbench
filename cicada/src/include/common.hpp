#ifndef COMMON_HPP
#define COMMON_HPP

#include "int64byte.hpp"
#include "lock.hpp"
#include "procedure.hpp"
#include "tuple.hpp"
#include "version.hpp"
#include <pthread.h>
#include <iostream>
#include <atomic>
#include <queue>

#ifdef GLOBAL_VALUE_DEFINE
	#define GLOBAL

GLOBAL std::atomic<unsigned int> Running(0);
GLOBAL std::atomic<unsigned int> Ending(0);
GLOBAL std::atomic<bool> Finish(false);
GLOBAL std::atomic<uint64_t> MinRts(0);
GLOBAL std::atomic<uint64_t> MinWts(0);
GLOBAL std::atomic<unsigned int> FirstAllocateTimestamp(0);

#else
	#define GLOBAL extern

GLOBAL std::atomic<unsigned int> Running;
GLOBAL std::atomic<unsigned int> Ending;
GLOBAL std::atomic<bool> Finish;
GLOBAL std::atomic<uint64_t> MinRts;
GLOBAL std::atomic<uint64_t> MinWts;
GLOBAL std::atomic<unsigned int> FirstAllocateTimestamp;

#endif

GLOBAL unsigned int TUPLE_NUM;
GLOBAL unsigned int MAX_OPE;
GLOBAL unsigned int THREAD_NUM;
GLOBAL Workload WORKLOAD;
GLOBAL bool P_WAL;
GLOBAL bool S_WAL;
GLOBAL bool ELR;	//early lock release
GLOBAL bool NLR;
GLOBAL uint64_t GROUP_COMMIT;
GLOBAL uint64_t CLOCK_PER_US;	//US = micro(µ) seconds 
GLOBAL double IO_TIME_NS;	//nano second
GLOBAL int GROUP_COMMIT_TIMEOUT_US;	//micro seconds
GLOBAL int GARBAGE_COLLECTION_INTERVAL_US;	//micro seconds
GLOBAL uint64_t EXTIME;
GLOBAL unsigned int UNSTABLE_WORKLOAD;

GLOBAL TimeStamp *ThreadWts;
GLOBAL TimeStamp *ThreadRts;
GLOBAL TimeStamp **ThreadWtsArray;
GLOBAL TimeStamp **ThreadRtsArray;
GLOBAL uint64_t_64byte *ThreadRtsArrayForGroup;	//グループコミットをする時，これが必要である．

GLOBAL uint64_t_64byte *GROUP_COMMIT_INDEX;
GLOBAL uint64_t_64byte *GROUP_COMMIT_COUNTER;	//s-walの時は[0]のみ使用。全スレッドで共有。
GLOBAL TimeStamp *ThreadFlushedWts;

GLOBAL Version ***PLogSet;	//[thID][index] pointer array
GLOBAL Version **SLogSet;	//[index] pointer array
GLOBAL RWLock SwalLock;

GLOBAL uint64_t_64byte *FinishTransactions;
GLOBAL uint64_t_64byte *AbortCounts;

GLOBAL uint64_t Bgn;
GLOBAL uint64_t End;
GLOBAL uint64_t_64byte *GCFlag;
GLOBAL uint64_t_64byte *GCommitStart;
GLOBAL uint64_t_64byte *GCommitStop;

GLOBAL Tuple *Table;
GLOBAL uint64_t InitialWts;

#define SPIN_WAIT_TIMEOUT_US 2
#define GC_INTER_US 10 

#endif	//	COMMON_HPP
