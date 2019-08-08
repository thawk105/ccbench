#pragma once
#include <atomic>
#include <cstdint>

#include "../../include/cache_line_size.hh"

#include "version.hh"

using namespace std;

class Tuple {
 public:
  alignas(CACHE_LINE_SIZE) 
#if INLINE_VERSION_OPT
  Version inline_version_;
#endif
  atomic<Version *> latest_;
  atomic<uint64_t> min_wts_;
  atomic<uint8_t> gClock_;

  Tuple() : latest_(nullptr), gClock_(0) {}

  Version *ldAcqLatest() { return latest_.load(std::memory_order_acquire); }

  bool getGCRight(uint8_t thid) {
    uint8_t expected, desired(thid);
    expected = this->gClock_.load(std::memory_order_acquire);
    for (;;) {
      if (expected != 0) return false;
      if (this->gClock_.compare_exchange_weak(expected, desired,
                                              std::memory_order_acq_rel,
                                              std::memory_order_acquire))
        return true;
    }
  }

  void returnGCRight() { this->gClock_.store(0, std::memory_order_release); }

#if INLINE_VERSION_OPT
  // inline
  bool getInlineVersionRight() {
    VersionStatus expected, desired(VersionStatus::pending);
    expected = this->inline_version_.status_.load(std::memory_order_acquire);
    for (;;) {
      if (expected != VersionStatus::unused) return false;
      if (this->inline_version_.status_.compare_exchange_weak(
              expected, desired, std::memory_order_acq_rel,
              std::memory_order_acquire))
        return true;
    }
  }

  void returnInlineVersionRight() {
    this->inline_version_.status_.store(VersionStatus::unused,
                                        std::memory_order_release);
  }
#endif
};
