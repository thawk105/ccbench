#pragma once

#include <atomic>
#include <cstdint>
#include <utility>
#include <xmmintrin.h>

#include "debug.hpp"

extern bool chkClkSpan(uint64_t &start, uint64_t &stop, uint64_t threshold);

enum class LMode : uint8_t {
	reader, 
	writer
};

enum class LStatus : uint8_t {
	waiting,
	granted,
	leaving
};

enum class MQL_RESULT : uint8_t {
	LockAquired,
	LockCancelled
};

class MQLnode {
public:
	// interact with predecessor
	LMode type;
	MQLnode *prev;
	std::atomic<bool> granted;
	// -----
	// interact with successor
	struct MQL_sucInfo {
		MQLnode *next;
		bool busy;
		LMode stype;
		LStatus status;
	} sucInfo;
	// -----
};
	
class MQLock {
public:
	std::atomic<unsigned int> nreaders;
	MQLnode *tail;
	MQLnode *next_writer;

	MQLock() {
		nreaders = 0;
		tail = nullptr;
		next_writer = nullptr;
	}

	MQL_RESULT reader_acquire(MQLnode *qnode, bool trylock);
	MQL_RESULT finish_reader_acquire(MQLnode *qnode);
	MQL_RESULT reader_cancel(MQLnode *qnode);

	MQL_RESULT writer_acquire(MQLnode *qnode, bool trylock);
	MQL_RESULT finish_writer_acquire(MQLnode *qnode);
	MQL_RESULT writer_cancel(MQLnode *qnode);
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
