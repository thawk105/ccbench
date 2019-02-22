#pragma once

#include <atomic>
#include <cstdint>
#include "version.hpp"

class Tuple {
public: 
  std::atomic<Version *> latest;
  std::atomic<uint32_t> min_cstamp;
  std::atomic<uint8_t> gClock;
  char pad[15];
  // size to here is 28;

  char keypad[KEY_SIZE];

  Tuple() {
    latest.store(nullptr);
    gClock.store(0, std::memory_order_release);
  }
};
