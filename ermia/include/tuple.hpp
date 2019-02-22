#pragma once

#include <atomic>
#include <cstdint>
#include "version.hpp"

class Tuple {
public: 
  std::atomic<Version *> latest;
  std::atomic<uint32_t> min_cstamp;
  std::atomic<uint8_t> gClock;
  int8_t pad[15] = {};
  // size to here is 28 bytes

  int8_t keypad[KEY_SIZE] = {}; // virtual key. it can assume many key size.

  Tuple() {
    latest.store(nullptr);
    gClock.store(0, std::memory_order_release);
  }
};
