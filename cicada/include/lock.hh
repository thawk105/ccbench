#pragma once

#include <xmmintrin.h>
#include <atomic>

using namespace std;

class RWLock {
public:
  std::atomic<int> counter;
  // counter == -1, write locked;
  // counter == 0, not locked;
  // counter > 0, there are $counter readers who acquires read-lock.

  RWLock() { counter.store(0, std::memory_order_release); }

  // Read lock
  void r_lock() {
    int expected, desired;
    for (;;) {
      expected = counter.load(std::memory_order_acquire);
RETRY_R_LOCK:
      if (expected != -1)
        desired = expected + 1;
      else {
        continue;
      }
      if (counter.compare_exchange_strong(
              expected, desired, memory_order_acq_rel, memory_order_acquire))
        break;
      else
        goto RETRY_R_LOCK;
    }
  }

  void r_unlock() { counter--; }

  // Write lock
  void w_lock() {
    int expected;
    for (;;) {
      expected = counter.load(memory_order_acquire);
RETRY_W_LOCK:
      if (expected != 0) continue;
      if (counter.compare_exchange_strong(expected, -1, memory_order_acq_rel,
                                          memory_order_acquire))
        break;
      else
        goto RETRY_W_LOCK;
    }
  }

  void w_unlock() { counter++; }

  // Upgrae, read -> write
  void upgrade() {
    int one = 1;
    while (!counter.compare_exchange_strong(one, -1, memory_order_acq_rel,
                                            memory_order_acquire)) {
    }
  }
};
