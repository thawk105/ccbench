#ifndef TRANSACTION_HPP
#define TRANSACTION_HPP

#include "/home/tanabe/package/tbb/include/tbb/scalable_allocator.h"

#include <cstdint>
#include <map>

#include "version.hpp"

enum class TransactionStatus : uint8_t {
	inFlight,
	committing,
	committed,
	aborted,
};

class Transaction {
public:
	uint64_t cstamp = 0;	// Transaction end time, c(T)	
	TransactionStatus status = TransactionStatus::inFlight;		// Status: inFlight, committed, or aborted
	std::map<unsigned int, Version *, std::less<unsigned int>, tbb::scalable_allocator<Version *>> readSet;		// Non-overwritten read set
	std::map<unsigned int, Version *, std::less<unsigned int>, tbb::scalable_allocator<Version *>> writeSet;	// Write set

	uint8_t thid;	// thread ID
	uint64_t txid;	//TID and begin timestamp - the current log sequence number (LSN)

	Transaction(unsigned int myid) {
		this->thid = myid;
	}

	void tbegin();
	unsigned int tread(unsigned int key);
	void twrite(unsigned int key, unsigned int val);
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
