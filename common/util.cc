
#include <stdio.h>
#include <sys/time.h>
#include <xmmintrin.h>

#include <atomic>
#include <vector>

#include "../include/atomic_wrapper.hpp"

bool
chkSpan(struct timeval &start, struct timeval &stop, long threshold)
{
  long diff = 0;
  diff += (stop.tv_sec - start.tv_sec) * 1000 * 1000 + (stop.tv_usec - start.tv_usec);
  if (diff > threshold) return true;
  else return false;
}

bool
chkClkSpan(const uint64_t start, const uint64_t stop, const uint64_t threshold)
{
  uint64_t diff = 0;
  diff = stop - start;
  if (diff > threshold) return true;
  else return false;
}

void
waitForReadyOfAllThread(std::atomic<unsigned int> &running, const unsigned int thnum)
{
  unsigned int expected, desired;
  expected = running.load(std::memory_order_acquire);
  do {
    desired = expected + 1;
  } while (!running.compare_exchange_weak(expected, desired, std::memory_order_acq_rel, std::memory_order_acquire));

  while (running.load(std::memory_order_acquire) != thnum) _mm_pause;

  return;
}

bool
isReady(const std::vector<char>& readys)
{
  for (const char& b : readys) {
    if (!loadAcquire(b)) return false;
  }
  return true;
}

void
waitForReady(const std::vector<char>& readys)
{
  while (!isReady(readys)) {
    _mm_pause();
  }
}

