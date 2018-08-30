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
	uint64_t pstamp = 0;	// Predecessor high-water mark, η (T)
	uint64_t sstamp = UINT64_MAX;	// Successor low-water mark, pi (T)
	std::map<int, Version *, std::less<int>, tbb::scalable_allocator<Version *>> readSet;		// Non-overwritten read set
	std::map<int, Version *, std::less<int>, tbb::scalable_allocator<Version *>> writeSet;	// Write set

	uint8_t thid;	// thread ID
	uint64_t txid;	//TID and begin timestamp - the current log sequence number (LSN)
	bool safeRetry = false;

	void tbegin(const int &thid);
	int ssn_tread(int key);
	void ssn_twrite(int key, int val);
	void ssn_commit();
	void ssn_parallel_commit();
	void abort();
	void rwsetClear();
	void verify_exclusion_or_abort();
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

#endif	//	TRANSACTION_HPP
