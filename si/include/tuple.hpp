#pragma once

#include <atomic>
#include <cstdint>
#include "version.hpp"

class Tuple {
public: 
  std::atomic<Version *> latest;
  std::atomic<uint32_t> min_cstamp;
  unsigned int key;
  std::atomic<uint8_t> gClock;

  Tuple() {
    latest.store(nullptr);
    gClock.store(0, std::memory_order_release);
  }
};
