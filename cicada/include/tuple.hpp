#pragma once
#include <atomic>
#include <cstdint>

#include "../../include/cache_line_size.hpp"

#include "version.hpp"

using namespace std;

class Tuple {
public:
  atomic<Version *> latest;
  atomic<uint64_t> min_wts;
  Version inlineVersion;
  atomic<uint8_t> gClock; 

  int8_t keypad[KEY_SIZE]; // virtual key. it can assume many key size.

  int8_t pad[CACHE_LINE_SIZE - ((17 + KEY_SIZE + sizeof(Version)) % (CACHE_LINE_SIZE))];
  // it is description for convenience.
  // 17 : latest(8) + min_wts(8) + gClock(1)

  Tuple() {
    latest.store(nullptr);
    gClock.store(0, memory_order_release);
  }

  Version *ldAcqLatest() {
    return latest.load(std::memory_order_acquire);
  }
};
