#pragma once

#include "/home/tanabe/package/tbb/include/tbb/scalable_allocator.h"

#include <iostream>
#include <set>
#include <vector>

#include "common.hpp"
#include "log.hpp"
#include "procedure.hpp"
#include "tuple.hpp"

#define LOGSET_SIZE 1000

using namespace std;

class TxnExecutor {
public:
	vector<ReadElement> readSet;
	vector<WriteElement> writeSet;

  vector<LogPack> logSet;
  LogPack latestLogPack;

	unsigned int thid;
	Tidword mrctid;
	Tidword max_rset, max_wset;

	uint64_t finishTransactions;
	uint64_t abortCounts;

	TxnExecutor(int newthid) : thid(newthid) {
		readSet.reserve(MAX_OPE);
		writeSet.reserve(MAX_OPE);
    logSet.reserve(LOGSET_SIZE);

    latestLogPack.init();

		max_rset.obj = 0;
		max_wset.obj = 0;
		finishTransactions = 0;
		abortCounts = 0;
	}

	void tbegin();
	int tread(unsigned int key);
	void twrite(unsigned int key, unsigned int val);
	bool validationPhase();
	void abort();
	void writePhase();
  void wal();
	void lockWriteSet();
	void unlockWriteSet();
	ReadElement *searchReadSet(unsigned int key);
	WriteElement *searchWriteSet(unsigned int key);
};
