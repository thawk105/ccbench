#pragma once

#include <string.h> // memcpy
#include <sys/time.h>

#include <atomic>
#include <cstdint>

#include "../../include/cache_line_size.hpp"
#include "../../include/op_element.hpp"

#include "timeStamp.hpp"

using namespace std;

enum class VersionStatus : uint8_t {
  invalid,
  pending,
  aborted,
  precommitted, //early lock releaseで利用
  committed,
  deleted,
  unused,
};

class Version {
public:
  alignas(CACHE_LINE_SIZE)
  atomic<uint64_t> rts_;
  atomic<uint64_t> wts_;
  atomic<Version *> next_;
  atomic<VersionStatus> status_; //commit record
  // version size to here is 25 bytes.

  char val_[VAL_SIZE];

  Version() {
    status_.store(VersionStatus::pending, memory_order_release);
    next_.store(nullptr, memory_order_release);
  }

  Version(uint64_t rts, uint64_t wts) {
    rts_.store(rts, memory_order_relaxed);
    wts_.store(wts, memory_order_relaxed);;
  }

  void set(uint64_t rts, uint64_t wts) {
    rts_ = rts;
    wts_ = wts;
  }

  void set(uint64_t rts, uint64_t wts, Version *next, VersionStatus status) {
    rts_ = rts;
    wts_ = wts;
    next_ = next;
    status_ = status;
  }

  uint64_t ldAcqRts() {
    return rts_.load(std::memory_order_acquire);
  }

  uint64_t ldAcqWts() {
    return wts_.load(std::memory_order_acquire);
  }

  Version *ldAcqNext() {
    return next_.load(std::memory_order_acquire);
  }

  VersionStatus ldAcqStatus() {
    return status_.load(std::memory_order_acquire);
  }

  void strRelNext(Version *next) {
    next_.store(next, std::memory_order_release);
  }
};

