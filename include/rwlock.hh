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

  RWLock() { counter.store(0, memory_order_release); }

  void init() { counter.store(0, memory_order_release); }

  // Read lock
  void r_lock() {
    int expected, desired;
    expected = counter.load(memory_order_acquire);
    for (;;) {
      if (expected != -1)
        desired = expected + 1;
      else {
        expected = counter.load(memory_order_acquire);
        continue;
      }

      if (counter.compare_exchange_strong(
              expected, desired, memory_order_acq_rel, memory_order_acquire))
        return;
    }
  }

  bool r_trylock() {
    int expected, desired;
    expected = counter.load(memory_order_acquire);
    for (;;) {
      if (expected != -1)
        desired = expected + 1;
      else
        return false;

      if (counter.compare_exchange_strong(
              expected, desired, memory_order_acq_rel, memory_order_acquire))
        return true;
    }
  }

  void r_unlock() { counter--; }

  void w_lock() {
    int expected, desired(-1);
    expected = counter.load(memory_order_acquire);
    for (;;) {
      if (expected != 0) {
        expected = counter.load(memory_order_acquire);
        continue;
      }
      if (counter.compare_exchange_strong(
              expected, desired, memory_order_acq_rel, memory_order_acquire))
        return;
    }
  }

  bool w_trylock() {
    int expected, desired(-1);
    expected = counter.load(memory_order_acquire);
    for (;;) {
      if (expected != 0) return false;

      if (counter.compare_exchange_strong(
              expected, desired, memory_order_acq_rel, memory_order_acquire))
        return true;
    }
  }

  void w_unlock() { counter++; }

  // Upgrae, read -> write
  void upgrade() {
    int expected, desired(-1);
    expected = counter.load(memory_order_acquire);
    for (;;) {
      if (expected != 1) {
        expected = counter.load(memory_order_acquire);
        continue;
      }

      if (counter.compare_exchange_strong(
              expected, desired, memory_order_acq_rel, memory_order_acquire))
        return;
    }
  }

  bool tryupgrade() {
    int expected, desired(-1);
    expected = counter.load(memory_order_acquire);
    for (;;) {
      if (expected != 1) return false;

      if (counter.compare_exchange_strong(expected, desired,
                                          std::memory_order_acq_rel))
        return true;
    }
  }
};
