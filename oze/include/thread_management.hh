#pragma once

#include <atomic>
#include <cstdint>

#include "../../include/cache_line_size.hh"
#include "transaction_status.hh"

class ThreadManagementEntry {
  public:
  ThreadManagementEntry(uint64_t epoch, uint8_t mode) {
    this->epoch_.store(epoch, std::memory_order_relaxed);
    this->mode_.store(mode, std::memory_order_relaxed);
    this->last_long_tx_epoch_.store(0, std::memory_order_relaxed);
  }

  void atomicStoreEpoch(uint64_t epoch) {
    this->epoch_.store(epoch, std::memory_order_release);
  }

  void atomicStoreMode(uint8_t mode) {
    this->mode_.store(mode, std::memory_order_release);
  }

  uint64_t atomicLoadEpoch() {
    return this->epoch_.load(std::memory_order_acquire);
  }

  uint8_t atomicLoadMode() {
    return this->mode_.load(std::memory_order_acquire);
  }

  private:
  alignas(CACHE_LINE_SIZE) std::atomic<uint64_t> epoch_;
  std::atomic<uint8_t> mode_;
  std::atomic<uint64_t> last_long_tx_epoch_;
};
