#pragma once

#include <cstdint>
#include <map>
#include <vector>

#include "version.hpp"
#include "/home/tanabe/package/tbb/include/tbb/scalable_allocator.h"

enum class TransactionStatus : uint8_t {
	inFlight,
	committing,
	committed,
	aborted,
};

using namespace std;

class Transaction {
public:
	uint64_t cstamp = 0;	// Transaction end time, c(T)	
	TransactionStatus status = TransactionStatus::inFlight;		// Status: inFlight, committed, or aborted
	uint64_t pstamp = 0;	// Predecessor high-water mark, η (T)
	uint64_t sstamp = UINT64_MAX;	// Successor low-water mark, pi (T)
	vector<SetElement> readSet;
	vector<SetElement> writeSet;

	uint8_t thid;	// thread ID
	uint64_t txid;	//TID and begin timestamp - the current log sequence number (LSN)
	bool safeRetry = false;

	Transaction(uint8_t thid, unsigned int max_ope) {
		this->thid = thid;
		readSet.reserve(max_ope);
		writeSet.reserve(max_ope);
	}

	SetElement *searchReadSet(unsigned int key);
	SetElement *searchWriteSet(unsigned int key);
	void tbegin();
	int ssn_tread(unsigned int key);
	void ssn_twrite(unsigned int key, unsigned int val);
	void ssn_commit();
	void ssn_parallel_commit();
	void abort();
	void verify_exclusion_or_abort();
	void dispWS();
	void dispRS();
};

// for MVCC SSN
class TransactionTable {
public:
	std::atomic<uint64_t> cstamp;
	std::atomic<uint64_t> sstamp;
	std::atomic<uint64_t> lastcstamp;
	std::atomic<uint64_t> prev_cstamp;
	std::atomic<TransactionStatus> status;
};

