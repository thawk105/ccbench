#pragma once

#include <atomic>
#include <cstdint>
#include <utility>
#include <xmmintrin.h>

#include "debug.hpp"

using namespace std;

enum class LMode : uint8_t {
	none,
	reader, 
	writer
};

enum class LStatus : uint8_t {
	waiting,
	granted,
	leaving
};

class MQLock {
public:
	// interact with predecessor
	LMode lmode;
	bool granted;
	// -----
	// interact with successor
	bool busy;
	LMode stype;
	LStatus status;
	// -----
	MQLock *prev;
	MQLock *next;

	// return bool show whether lock operation success or fail
	void r_lock();
	bool r_trylock();
	void r_unlock();
	void w_lock();
	bool w_trylock();
	void w_unlock();
};
	
class RWLock {
public:
	atomic<int> counter;
	// counter == -1, write locked;
	// counter == 0, not locked;
	// counter > 0, there are $counter readers who acquires read-lock.
	
	RWLock() { counter.store(0, memory_order_release);}
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
