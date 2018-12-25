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
	Tidword mrctid;
	Tidword max_rset;
	Tidword max_wset;
	Xoroshiro128Plus *rnd;
	int locknum; // corresponding to index of MQLNodeList.

	Transaction(int thid, Xoroshiro128Plus *rnd, int locknum) {
		readSet.reserve(MAX_OPE);
		writeSet.reserve(MAX_OPE);
		RLL.reserve(MAX_OPE);
		CLL.reserve(MAX_OPE);

		this->thid = thid;
		this->status = TransactionStatus::inFlight;
		this->rnd = rnd;
		max_rset.obj = 0;
		max_wset.obj = 0;
		this->locknum = locknum;
	}

	ReadElement *searchReadSet(unsigned int key);
	WriteElement *searchWriteSet(unsigned int key);
	LockElement *searchRLL(unsigned int key);
	void removeFromCLL(LockElement *le);
	void begin();
	unsigned int read(unsigned int key);
	void write(unsigned int key, unsigned int val);
	void lock(Tuple *tuple, bool mode);
	void construct_RLL();	// invoked on abort;
	void unlockCLL();
	bool commit();
	void abort();
	void writePhase();
	void dispCLL();
	void dispRLL();
	void dispWS();
};

