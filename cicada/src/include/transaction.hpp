#pragma once

#include "tuple.hpp"
#include "procedure.hpp"
#include "common.hpp"
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
	TimeStamp *rts;
	TimeStamp *wts;
	bool ronly;
	vector<ReadElement> readSet;
	vector<WriteElement> writeSet;
	queue<GCElement> gcq;

	uint64_t start, stop;	// for one-sided synchronization
	uint64_t GCstart, GCstop; // for garbage collection
	uint8_t thid;

	Transaction(TimeStamp *thrts, TimeStamp *thwts, unsigned int thid) {
		this->rts = thrts;
		this->wts = thwts;
		this->wts->generateTimeStampFirst(thid);
		this->wts->ts = InitialWts + 2;
		this->thid = thid;
		this->ronly = false;

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
