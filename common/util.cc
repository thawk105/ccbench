
#include <stdio.h>
#include <sys/time.h>
#include <xmmintrin.h>

#include <atomic>
#include <thread>
#include <vector>

#include "../include/atomic_wrapper.hpp"
#include "../include/debug.hpp"

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

size_t
decide_parallel_build_number(size_t tuplenum)
{
  for (size_t i = std::thread::hardware_concurrency(); i > 0; --i) {
    if (tuplenum % i == 0) {
      return i;
    }
    if (i == 1) ERR;
  }

  return 0;
}

void
ReadyAndWaitForReadyOfAllThread(std::atomic<size_t> &running, const size_t thnm)
{
  running++;
  while (running.load(std::memory_order_acquire) != thnm) _mm_pause();

  return;
}

void
waitForReadyOfAllThread(std::atomic<size_t> &running, const size_t thnm)
{
  while (running.load(std::memory_order_acquire) != thnm) _mm_pause();

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

void
sleepMs(size_t ms)
{
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

