#ifndef LOCK_HPP
#define LOCK_HPP

#include <atomic>

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
		do {
			expected = counter.load(memory_order_acquire);
			while (expected == -1) 
				expected = counter.load(memory_order_acquire);
			desired = expected + 1;
		} while (!counter.compare_exchange_strong(expected, desired, memory_order_acq_rel));

		return;
	}

	bool r_trylock() {
		// success return true;
		// fail	   return false;
		int expected, desired;
		do {
			expected = counter.load(memory_order_acquire);
			if (expected == -1) return false;
			desired = expected + 1;
		} while (!counter.compare_exchange_strong(expected, desired, memory_order_acq_rel));

		return;
	}

	void r_unlock() {
		counter--;
		return;
	}

	// Write lock
	void w_lock() {
		int zero = 0;
		while(!counter.compare_exchange_strong(zero, -1, memory_order_acq_rel)) {}
		return;
	}

	bool w_trylock() {
		int zero = 0;
		return counter.compare_exchange_strong(zero, -1, memory_order_acq_rel);
	}

	void w_unlock() {
		counter++;
		return;
	}

	// Upgrae, read -> write
	void upgrade() {
		int one = 1;
		while (!counter.compare_exchange_strong(one, -1, memory_order_acq_rel)) {}
		return;
	}
};

// ロックリストに使用
// thread-local に使用するため，アライメント気にしない
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
};
		
#endif	// LOCK_HPP
