#ifndef LOCK_HPP
#define LOCK_HPP

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
		int expected;
		do {
			expected = counter.load(memory_order_acquire);
			while (expected == -1) {
				expected = counter.load(memory_order_acquire);
			}
		} while (!counter.compare_exchange_weak(expected, expected + 1, memory_order_acq_rel));
		return;
	}

	bool r_trylock() {
		// success return true;
		// fail	   return false;
		int expected;
		do {
			expected = counter.load(memory_order_acquire);
			if (expected == -1) return false;
		} while (!counter.compare_exchange_weak(expected, expected + 1, memory_order_acq_rel));

		return true;
	}

	void r_unlock() {
		counter--;
	}

	// Write lock
	void w_lock() {
		int expected, desired(-1);
		do {
			expected = counter.load(memory_order_acquire);
			while (expected != 0) {
				expected = counter.load(memory_order_acquire);
			}
		} while (!counter.compare_exchange_weak(expected, desired, memory_order_acq_rel));
		return;
	}

	bool w_trylock() {
		int zero = 0;
		return counter.compare_exchange_weak(zero, -1, memory_order_acq_rel);
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
		
#endif	// LOCK_HPP
