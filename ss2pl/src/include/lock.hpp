#ifndef LOCK_HPP
#define LOCK_HPP

#include <atomic>

class RWLock {
public:
	std::atomic<int> counter;
	// counter == -1, write locked;
	// counter == 0, not locked;
	// counter > 0, there are $counter readers who acquires read-lock.
	
	RWLock() {
		counter.store(0, std::memory_order_release);
	}
	// Read lock
	bool r_lock() {
		int expected, desired;
		do {
			expected = counter.load(std::memory_order_acquire);
			if (expected != -1) desired = expected + 1;
			else {
				return false;
			}
		} while (!counter.compare_exchange_strong(expected, desired, std::memory_order_acq_rel));

		return true;
	}
	void r_unlock() {
		counter--;
	}
	// Write lock
	bool w_lock() {
		// 成功したら true;
		// 失敗したら false;
		int zero = 0;
		return counter.compare_exchange_strong(zero, -1, std::memory_order_acq_rel);
	}
	void w_unlock() {
		counter++;
	}
	// Upgrae, read -> write
	bool upgrade() {
		// 成功したら true;
		// 失敗したら false;
		int one = 1;
		return counter.compare_exchange_strong(one, -1, std::memory_order_acq_rel);
	}
};
		
#endif
