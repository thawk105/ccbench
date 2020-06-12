#pragma once

#include <atomic>
#include <cstdint>

#include "../../include/cache_line_size.hh"
#include "version.hh"

class Tuple {
public:
  alignas(CACHE_LINE_SIZE) std::atomic<Version *> latest_;
  std::atomic <uint32_t> min_cstamp_;
  std::atomic <uint8_t> gc_lock_;

  Tuple() {
    latest_.store(nullptr);
    gc_lock_.store(0, std::memory_order_release);
  }
};
