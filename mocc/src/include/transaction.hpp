#pragma once

#include "lock.hpp"
#include "tuple.hpp"
#include "common.hpp"
#include <vector>

using namespace std;

enum class TransactionStatus : uint8_t {
	inFlight,
	committed,
	aborted,
};

class Transaction {
public:
	vector<ReadElement> readSet;
	vector<WriteElement> writeSet;
	vector<LockElement> RLL;
	vector<LockElement> CLL;
	TransactionStatus status;

	int thid;
	Xoroshiro128Plus *rnd;

	Transaction(int thid, Xoroshiro128Plus *rnd) {
		readSet.reserve(MAX_OPE);
		writeSet.reserve(MAX_OPE);
		RLL.reserve(MAX_OPE);
		CLL.reserve(MAX_OPE);

		this->thid = thid;
		this->status = TransactionStatus::inFlight;
		this->rnd = rnd;
	}

	ReadElement *searchReadSet(unsigned int key);
	WriteElement *searchWriteSet(unsigned int key);
	LockElement *searchRLL(unsigned int key);
	void begin();
	unsigned int read(unsigned int key);
	void write(unsigned int key, unsigned int val);
	void lock(Tuple *tuple, bool mode);
	void construct_rll();	// invoked on abort;
	void unlockCLL();
	bool commit();
	void abort();
	void writePhase();
	void dispCLL();
	void dispRLL();
	void dispWS();
};

