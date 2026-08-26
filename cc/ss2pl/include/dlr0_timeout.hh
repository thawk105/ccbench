#pragma once

#if defined(DLR0)

#include <atomic>
#include <cstdint>

#include "../../../include/tsc.hh"
#include "common.hh"

inline bool ss2pl_dlr0_try_read_once(ReaderWriteLock& lock) {
  int expected = lock.counter.load(std::memory_order_acquire);
  if (expected == -1) return false;

  return lock.counter.compare_exchange_strong(expected, expected + 1,
                                              std::memory_order_acq_rel,
                                              std::memory_order_acquire);
}

inline bool ss2pl_dlr0_try_write_once(ReaderWriteLock& lock) {
  int expected = lock.counter.load(std::memory_order_acquire);
  if (expected != 0) return false;

  return lock.counter.compare_exchange_strong(
      expected, -1, std::memory_order_acq_rel, std::memory_order_acquire);
}

inline bool ss2pl_dlr0_try_upgrade_once(ReaderWriteLock& lock) {
  int expected = lock.counter.load(std::memory_order_acquire);
  if (expected != 1) return false;

  return lock.counter.compare_exchange_strong(
      expected, -1, std::memory_order_acq_rel, std::memory_order_acquire);
}

template <typename TryOnce>
inline bool ss2pl_dlr0_lock_until_timeout(TryOnce&& try_once) {
  const std::uint64_t timeout_clocks =
      FLAGS_ss2pl_dlr0_timeout_us * FLAGS_clocks_per_us;
  const std::uint64_t start = rdtscp();

  for (;;) {
    if (try_once()) return true;
    if (rdtscp() - start >= timeout_clocks) return false;
    _mm_pause();
  }
}

#endif
