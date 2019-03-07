#pragma once

#include <atomic>
#include <cstdint>

#include "../../include/cache_line_size.hpp"

#include "version.hpp"

class Tuple {
public: 
  std::atomic<Version *> latest;
  std::atomic<uint32_t> min_cstamp;
  std::atomic<uint8_t> gClock;
  // size to here is 13 bytes

  int8_t keypad[KEY_SIZE]; // virtual key. it can assume many key size.

  int8_t pad[CACHE_LINE_SIZE - ((13 + KEY_SIZE) % CACHE_LINE_SIZE)];

  Tuple() {
    latest.store(nullptr);
    gClock.store(0, std::memory_order_release);
  }
};
