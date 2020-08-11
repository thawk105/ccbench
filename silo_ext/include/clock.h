/**
 * @file clock.h
 * @brief functions about clock.
 */

#pragma once

#include <cstdint>
#include <thread>

namespace ccbench {

[[maybe_unused]] static bool check_clock_span(uint64_t &start,  // NOLINT
                                              uint64_t &stop,
                                              uint64_t threshold) {
  uint64_t diff{stop - start};
  return diff > threshold;
}

[[maybe_unused]] static void sleepMs(size_t ms) {
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

} // namespace ccbench
