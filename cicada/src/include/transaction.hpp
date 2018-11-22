#pragma once

#include "common.hpp"
#include "debug.hpp"
#include "procedure.hpp"
#include "tuple.hpp"
#include "timeStamp.hpp"
#include "version.hpp"

#include <atomic>
#include <iostream>
#include <map>
#include <queue>

using namespace std;

enum class TransactionStatus : uint8_t {
	invalid,
	inflight,
	commit,
	abort,
};

class Transaction {
public:
	TransactionStatus status = TransactionStatus::invalid;
	uint64_t rts;
	TimeStamp wts;
	bool ronly;
	vector<ReadElement> readSet;
	vector<WriteElement> writeSet;
	queue<GCElement> gcq;

	uint64_t start, stop;	// for one-sided synchronization
	uint64_t spinstart, spinstop; // for spin-wait
	uint64_t grpcmt_start, grpcmt_stop;	// for group commit
	uint64_t GCstart, GCstop; // for garbage collection
	uint8_t thid;

	uint64_t finish_transactions;
	uint64_t abort_counts;

	Transaction(unsigned int thid) {
		// wait to initialize MinWts
		while(MinWts.load(memory_order_acquire) == 0);
		this->rts = MinWts.load(memory_order_acquire) - 1;
		this->wts.generateTimeStampFirst(thid);
		this->thid = thid;
		this->ronly = false;

		__atomic_store_n(&(ThreadWtsArray[thid].num), this->wts.ts, __ATOMIC_RELEASE);
		unsigned int expected, desired;
		do {
			expected = FirstAllocateTimestamp.load(memory_order_acquire);
			desired = expected + 1;
		} while (!FirstAllocateTimestamp.compare_exchange_weak(expected, desired, memory_order_acq_rel));
		
		start = rdtsc();
		GCstart = start;
		readSet.clear();
		readSet.reserve(MAX_OPE);
		writeSet.clear();
		writeSet.reserve(MAX_OPE);

		finish_transactions = 0;
		abort_counts = 0;
	}

	void tbegin(bool ronly);
	int tread(unsigned int key);
	void twrite(unsigned int key, unsigned int val);
	bool validation();
	void writePhase();
	void swal();
	void pwal();	//parallel write ahead log.
	void precpv();	//pre-commit pending versions
	void cpv();	//commit pending versions
	void gcpv();	//group commit pending versions
	bool chkGcpvTimeout();
	void earlyAbort();
	void abort();
	void wSetClean();
	void displayWset();
	void mainte();	//maintenance
};
