#ifndef COMMON_HPP
#define COMMON_HPP

#include "int64byte.hpp"
#include "procedure.hpp"
#include "tuple.hpp"
#include "version.hpp"
#include "message.hpp"
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

#else
	#define GLOBAL extern

GLOBAL std::atomic<unsigned int> Running;
GLOBAL std::atomic<unsigned int> Ending;
GLOBAL std::atomic<bool> Finish;
GLOBAL std::atomic<uint64_t> MinRts;
GLOBAL std::atomic<uint64_t> MinWts;

#endif

GLOBAL unsigned int TUPLE_NUM;
GLOBAL unsigned int MAX_OPE;
GLOBAL unsigned int THREAD_NUM;
GLOBAL unsigned int PRO_NUM;
GLOBAL float READ_RATIO;
GLOBAL int SPIN_WAIT_TIMEOUT_US;	//US = micro(µ) seconds
GLOBAL bool P_WAL;
GLOBAL bool S_WAL;
GLOBAL bool ELR;	//early lock release
GLOBAL bool NLR;
GLOBAL uint64_t GROUP_COMMIT;
GLOBAL double CLOCK_PER_US;	//US = micro(µ) seconds 
GLOBAL double IO_TIME_NS;	//nano second
GLOBAL int GROUP_COMMIT_TIMEOUT_US;	//micro seconds
GLOBAL int GARBAGE_COLLECTION_INTERVAL_US;	//micro seconds
GLOBAL int EXTIME;

GLOBAL TimeStamp *ThreadWts;
GLOBAL TimeStamp *ThreadRts;
GLOBAL TimeStamp **ThreadWtsArray;
GLOBAL TimeStamp **ThreadRtsArray;
GLOBAL uint64_t_64byte *ThreadRtsArrayForGroup;	//グループコミットをする時，これが必要である．

GLOBAL std::atomic<bool> *FirstAllocateTimeStamp;
GLOBAL uint64_t_64byte *FinishTransactions;

GLOBAL uint64_t_64byte *GROUP_COMMIT_INDEX;
GLOBAL uint64_t_64byte *GROUP_COMMIT_COUNTER;	//s-walの時は[0]のみ使用。全スレッドで共有。
GLOBAL TimeStamp *ThreadFlushedWts;

GLOBAL Version ***PLogSet;	//[thID][index] pointer array
GLOBAL Version **SLogSet;	//[index] pointer array
GLOBAL pthread_mutex_t Lock;

GLOBAL std::queue<Message> *MessageQueue;	//コミット通知用

GLOBAL uint64_t_64byte *AbortCounts;

GLOBAL uint64_t Bgn;
GLOBAL uint64_t End;
GLOBAL uint64_t_64byte *Start;	//[thID] array. このタイマーはtbeginで利用している。
GLOBAL uint64_t_64byte *Stop;	//tbeginではone-sided-syncに利用しているため。専用タイマー
GLOBAL uint64_t_64byte *GCommitStart;
GLOBAL uint64_t_64byte *GCommitStop;	//Group Commitにて、前回の書き出しからの一定時間経過を検知
GLOBAL uint64_t_64byte *GCollectionStart;
GLOBAL uint64_t_64byte *GCollectionStop;	//Garbage Collectionにて、前回の処理からの一定時間経過を検知

GLOBAL Procedure **Pro;

GLOBAL Tuple *HashTable;

#endif	//	COMMON_HPP
