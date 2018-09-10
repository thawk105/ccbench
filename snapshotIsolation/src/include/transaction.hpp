#ifndef TRANSACTION_HPP
#define TRANSACTION_HPP

#include "/home/tanabe/package/tbb/include/tbb/scalable_allocator.h"

#include <cstdint>
#include <vector>

#include "common.hpp"
#include "version.hpp"

enum class TransactionStatus : uint8_t {
	inFlight,
	committed,
	aborted,
};

using namespace std;

class Transaction {
public:
	uint64_t cstamp = 0;	// Transaction end time, c(T)	
	TransactionStatus status = TransactionStatus::inFlight;		// Status: inFlight, committed, or aborted
	vector<ReadElement> readSet;
	vector<WriteElement> writeSet;

	uint8_t thid;	// thread ID
	uint64_t txid;	//TID and begin timestamp - the current log sequence number (LSN)

	Transaction(unsigned int myid, unsigned int max_ope) {
		readSet.reserve(max_ope);
		writeSet.reserve(max_ope);

		this->thid = myid;
	}

	void tbegin();
	unsigned int tread(unsigned int key);
	void twrite(unsigned int key, unsigned int val);
	bool lock(unsigned int key);
	bool commit();
	void abort();
	void rwsetClear();
};

// for MVCC SSN
class TransactionTable {
public:
	std::atomic<uint64_t> prev_cstamp;
};

#endif	//	TRANSACTION_HPP
