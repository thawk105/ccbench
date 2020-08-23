/**
 * @file clock.h
 * @brief functions about clock.
 */

#pragma once

#include <cstdint>
#include <thread>
#include <chrono>

namespace ccbench {

[[maybe_unused]] static bool check_clock_span(uint64_t &start,  // NOLINT
                                              uint64_t &stop,
                                              uint64_t threshold) {
  uint64_t diff{stop - start};
  return diff > threshold;
}

#if 0
[[maybe_unused]] static void sleepMs(size_t ms) {
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}
#endif


template<typename Clock = std::chrono::high_resolution_clock>
class StopWatch {
  using TimePoint = std::chrono::time_point<Clock>;
  TimePoint tp[2];
public:
  StopWatch() {
    tp[0] = Clock::now();
    tp[1] = tp[0];
  }

  void mark() {
    TimePoint now = Clock::now();
    tp[0] = tp[1];
    tp[1] = now;
  }

  double period() const {
    std::chrono::duration<double> ret = tp[1] - tp[0];
    return ret.count();
  }
};


} // namespace ccbench
