#pragma once

#include <atomic>
#include <utility>
#include <xmmintrin.h>

#include "debug.hpp"

using namespace std;

class RWLock {
public:
	atomic<int> counter;
	// counter == -1, write locked;
	// counter == 0, not locked;
	// counter > 0, there are $counter readers who acquires read-lock.
	
	RWLock() {
		counter.store(0, memory_order_release);
	}

	// Read lock
	void r_lock() {
		int expected, desired;
		for (;;) {
			expected = counter.load(memory_order_acquire);
RETRY_R_LOCK:
			if (expected != -1) desired = expected + 1;
			else {
				continue;
			}
			if (counter.compare_exchange_strong(expected, desired, memory_order_acq_rel, memory_order_acquire)) break;
			else goto RETRY_R_LOCK;
		}
	}

	bool r_trylock() {
		int expected, desired;
		for (;;) {
			expected = counter.load(memory_order_acquire);
RETRY_R_TRYLOCK:
			if (expected != -1) desired = expected + 1;
			else return false;

			if (counter.compare_exchange_strong(expected, desired, memory_order_acq_rel, memory_order_acquire)) return true;
			else goto RETRY_R_TRYLOCK;
		}
	}

	void r_unlock() {
		counter--;
	}

	// Write lock
	void w_lock() {
		int expected, desired(-1);
		for (;;) {
			expected = counter.load(memory_order_acquire);
RETRY_W_LOCK:
			if (expected != 0) continue;

			if (counter.compare_exchange_strong(expected, desired, memory_order_acq_rel, memory_order_acquire)) break;
			else goto RETRY_W_LOCK;
		}
	}

	bool w_trylock() {
		int expected, desired(-1);
		for (;;) {
			expected = counter.load(memory_order_acquire);
RETRY_W_TRYLOCK:
			if (expected != 0) return false;

			if (counter.compare_exchange_strong(expected, desired, memory_order_acq_rel, memory_order_acquire)) return true;
			else goto RETRY_W_TRYLOCK;
		}
	}

	void w_unlock() {
		counter++;
	}

	// Upgrae, read -> write
	bool upgrade() {
		int expected = 1;
		return counter.compare_exchange_weak(expected, -1, memory_order_acq_rel);
	}
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
