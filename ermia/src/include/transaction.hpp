#pragma once

#include <cstdint>
#include <map>
#include <vector>

#include "garbageCollection.hpp"
#include "version.hpp"
#include "/home/tanabe/package/tbb/include/tbb/scalable_allocator.h"

// forward declaration
class TransactionTable;

enum class TransactionStatus : uint8_t {
	inFlight,
	committing,
	committed,
	aborted,
};

using namespace std;

class Transaction {
public:
	uint32_t cstamp = 0;	// Transaction end time, c(T)	
	TransactionStatus status = TransactionStatus::inFlight;		// Status: inFlight, committed, or aborted
	uint32_t pstamp = 0;	// Predecessor high-water mark, η (T)
	uint32_t sstamp = UINT32_MAX;	// Successor low-water mark, pi (T)
	vector<SetElement> readSet;
	vector<SetElement> writeSet;
	GarbageCollection gcobject;
	uint32_t preGcThreshold = 0;

	uint8_t thid;	// thread ID
	uint32_t txid;	//TID and begin timestamp - the current log sequence number (LSN)

	Transaction(uint8_t thid, unsigned int max_ope) {
		this->thid = thid;
		readSet.reserve(max_ope);
		writeSet.reserve(max_ope);
	}

	SetElement *searchReadSet(unsigned int key);
	SetElement *searchWriteSet(unsigned int key);
	void tbegin();
	void upReadersBits(Version *ver);
	void downReadersBits(Version *ver);
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
	std::atomic<uint32_t> txid;
	std::atomic<uint32_t> cstamp;
	std::atomic<uint32_t> sstamp;
	std::atomic<uint32_t> lastcstamp;
	std::atomic<TransactionStatus> status;
	uint8_t padding[15];

	TransactionTable(uint32_t txid, uint32_t cstamp, uint32_t sstamp, uint32_t lastcstamp, TransactionStatus status) {
		this->txid = txid;
		this->cstamp = cstamp;
		this->sstamp = sstamp;
		this->lastcstamp = lastcstamp;
		this->status = status;
	}
};

