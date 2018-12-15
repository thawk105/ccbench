#pragma once

#include <atomic>
#include <cstdint>
#include <utility>
#include <xmmintrin.h>

#include "debug.hpp"

#define LOCK_TIMEOUT_US 5
// 5 us.
extern bool chkClkSpan(uint64_t &start, uint64_t &stop, uint64_t threshold);

enum class SentinelValue : uint32_t {
	None = 0,
	Acquired,	// 1
	SuccessorLeaving, // 2
	NoSuccessor
};

enum class LockMode : uint8_t {
	None,
	Reader, 
	Writer
};

enum class LockStatus : uint8_t {
	Waiting,
	Granted,
	Leaving
};

enum class MQL_RESULT : uint8_t {
	Acquired,
	Cancelled
};

struct MQLMetaInfo {
	union {
		uint64_t obj;
		struct {
			bool busy:1; // 0 == not busy, 1 == busy;
			LockMode stype:8; // 0 == none, 1 == reader, 2 == writer
			LockStatus status:8; // 0 == waiting, 1 == granted, 2 == leaving
			uint32_t next:32; // store a thrad id. and you know where the qnode;
			};
	};

	void init(bool busy, LockMode stype, LockStatus status, uint32_t next);
	bool atomicLoadBusy();
	LockMode atomicLoadStype();
	LockStatus atomicLoadStatus();
	uint32_t atomicLoadNext();
	void atomicStoreBusy(bool newbusy);
	void atomicStoreStatus(LockStatus newstatus);
	void atomicStoreNext(uint32_t newnext);
	bool atomicCASNext(uint32_t oldnext, uint32_t newnext);
};

class MQLNode {
public:
	// interact with predecessor
	std::atomic<LockMode> type;
	std::atomic<uint32_t> prev;
	std::atomic<bool> granted;
	// -----
	// interact with successor
	MQLMetaInfo sucInfo;
	// -----
	//
	void init(LockMode type, uint32_t prev, bool granted) {
		this->type = type;
		this->prev = prev;
		this->granted = granted;
	}
};
	
class MQLock {
public:
	std::atomic<unsigned int> nreaders;
	std::atomic<uint32_t> tail;
	std::atomic<uint32_t> next_writer;

	MQLock() {
		nreaders = 0;
		tail = 0;
		next_writer = 0;
	}

	MQL_RESULT acquire_reader_lock(uint32_t me, bool trylock);
	MQL_RESULT acquire_reader_lock_check_reader_pred(uint32_t me, uint32_t pred, bool trylock);
	MQL_RESULT acquire_reader_lock_check_writer_pred(uint32_t me, uint32_t pred, bool trylock);
	MQL_RESULT finish_acquire_reader_lock(uint32_t me);
	MQL_RESULT cancel_reader_lock(uint32_t me);

	MQL_RESULT acquire_writer_lock(uint32_t me, bool trylock);
	MQL_RESULT cancel_writer_lock(uint32_t me);
};

class RWLock {
public:
	std::atomic<int> counter;
	// counter == -1, write locked;
	// counter == 0, not locked;
	// counter > 0, there are $counter readers who acquires read-lock.
	
	RWLock() { counter.store(0, std::memory_order_release);}
	void r_lock(); // read lock
	bool r_trylock();	// read try lock
	void r_unlock();	// read unlock
	void w_lock();	// write lock
	bool w_trylock(); // write try lock
	void w_unlock();	// write unlock
	bool upgrade();	// upgrade from reader to writer
};

// for lock list
class LockElement {
public:
	unsigned int key;	// record を識別する．
	RWLock *lock;
	bool mode;	// 0 read-mode, 1 write-mode

	LockElement(unsigned int key, RWLock *lock, bool mode) {
		this->key = key;
		this->lock = lock;
		this->mode = mode;
	}

	bool operator<(const LockElement& right) const {
		return this->key < right.key;
	}

	// Copy constructor
	LockElement(const LockElement& other) {
		key = other.key;
		lock = other.lock;
		mode = other.mode;
	}
		
	// move constructor
	LockElement(LockElement && other) {
		key = other.key;
		lock = other.lock;
		mode = other.mode;
	}

	LockElement& operator=(LockElement&& other) noexcept {
		if (this != &other) {
			key = other.key;
			lock = other.lock;
			mode = other.mode;
		}
		return *this;
	}
};
