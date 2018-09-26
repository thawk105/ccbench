#pragma once

#include "int64byte.hpp"
#include "lock.hpp"
#include "procedure.hpp"
#include "transaction.hpp"
#include "tuple.hpp"

#include <mutex>
#include <vector>

#ifdef GLOBAL_VALUE_DEFINE
	#define GLOBAL
GLOBAL std::atomic<unsigned int> Running(0);
GLOBAL std::atomic<uint64_t> Lsn(0);
GLOBAL std::atomic<bool> Finish(false);

#else
	#define GLOBAL extern
GLOBAL std::atomic<unsigned int> Running;
GLOBAL std::atomic<uint64_t> Lsn;
GLOBAL std::atomic<bool> Finish;

#endif

//run-time args
GLOBAL unsigned int TUPLE_NUM;
GLOBAL unsigned int MAX_OPE;
GLOBAL unsigned int THREAD_NUM;
GLOBAL unsigned int WORKLOAD;
GLOBAL uint64_t CLOCK_PER_US;
GLOBAL unsigned int EXTIME;

GLOBAL uint64_t_64byte *ThtxID;
GLOBAL uint64_t *AbortCounts;
GLOBAL uint64_t *FinishTransactions;

GLOBAL uint64_t Bgn;
GLOBAL uint64_t End;

GLOBAL RWLock CtrLock;
GLOBAL Tuple *Table;
GLOBAL TransactionTable *TMT;	// Transaction Mapping Table

GLOBAL std::mutex SsnLock;
