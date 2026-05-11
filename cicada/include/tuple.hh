#pragma once

#include <atomic>
#include <cstdint>

#include "../../include/cache_line_size.hh"

#include "version.hh"
#include "time_stamp.hh"

using namespace std;

class TupleInitParam {
public:
  TimeStamp tstmp;
  uint64_t initial_wts;

  TupleInitParam() {
    tstmp.generateTimeStampFirst(0);
    initial_wts = tstmp.ts_;    
  }
};

class Tuple {
public:
  alignas(CACHE_LINE_SIZE)
#if INLINE_VERSION_OPT
  Version inline_ver_;
#endif
  atomic<Version *> latest_;
  atomic <uint64_t> min_wts_;
  atomic <uint64_t> continuing_commit_;
  atomic <uint8_t> gc_lock_;
  TupleBody body_; // only used for index tuple as single version

  Tuple() : latest_(nullptr), gc_lock_(0) {}

  Version *ldAcqLatest() { return latest_.load(std::memory_order_acquire); }

  bool getGCRight(uint8_t thid) {
    uint8_t expected, desired(thid);
    expected = this->gc_lock_.load(std::memory_order_acquire);
    for (;;) {
      if (expected != 0) return false;
      if (this->gc_lock_.compare_exchange_strong(expected, desired,
                                                 std::memory_order_acq_rel,
                                                 std::memory_order_acquire))
        return true;
    }
  }

  void returnGCRight() { this->gc_lock_.store(0, std::memory_order_release); }

#if INLINE_VERSION_OPT
  // inline
  bool getInlineVersionRight() {
    VersionStatus expected, desired(VersionStatus::pending);
    expected = this->inline_ver_.status_.load(std::memory_order_acquire);
    for (;;) {
      if (expected != VersionStatus::unused) return false;
      if (this->inline_ver_.status_.compare_exchange_strong(
              expected, desired, std::memory_order_acq_rel,
              std::memory_order_acquire))
        return true;
    }
  }

  void returnInlineVersionRight() {
    this->inline_ver_.status_.store(VersionStatus::unused,
                                    std::memory_order_release);
  }
#endif

  void init([[maybe_unused]] size_t thid, TupleBody&& body, TupleInitParam* param) {
    // for initializer
    min_wts_ = param->initial_wts;
    gc_lock_.store(0, std::memory_order_release);
    continuing_commit_.store(0, std::memory_order_release);

#if INLINE_VERSION_OPT
    latest_ = &inline_ver_;
    inline_ver_.set(0, param->initial_wts, nullptr, VersionStatus::committed);
    inline_ver_.body_ = std::move(body);
    body_ = std::ref(inline_ver_.body_);
#else
    latest_.store(new Version(), std::memory_order_release);
    (latest_.load(std::memory_order_acquire))
            ->set(0, param->initial_wts, nullptr, VersionStatus::committed);
    (latest_.load(std::memory_order_acquire))->body_ = std::move(body);
    body_ = std::ref((latest_.load(std::memory_order_acquire))->body_);
#endif
  }

  void init(size_t thid, Version* ver, uint64_t initial_wts, TupleBody&& body) {
    min_wts_ = initial_wts;
    gc_lock_.store(0, std::memory_order_release);
    continuing_commit_.store(0, std::memory_order_release);

#if INLINE_VERSION_OPT
    latest_ = &inline_ver_;
    body_ = std::ref(inline_ver_.body_);
#else
    latest_.store(ver, std::memory_order_release);
    body_ = std::ref((latest_.load(std::memory_order_acquire))->body_);
#endif
  }
};
