#pragma once

#include <atomic>
#include <cstdint>

#include "../../include/cache_line_size.hh"

enum class VersionStatus : uint8_t {
  inFlight,
  committed,
  aborted,
};

struct Psstamp {
  union {
    uint64_t obj_;
    struct {
      uint32_t pstamp;
      uint32_t sstamp;
    };
  };

  Psstamp() { obj_ = 0; }

  void init(uint32_t pstamp, uint32_t sstamp) {
    this->pstamp = pstamp;
    this->sstamp = sstamp;
  }

  uint64_t atomicLoad() {
    Psstamp expected;
    expected.obj_ = __atomic_load_n(&obj_, __ATOMIC_ACQUIRE);
    return expected.obj_;
  }

  bool atomicCASPstamp(uint32_t expectedPstamp, uint32_t desiredPstamp) {
    Psstamp expected, desired;
    expected.obj_ = __atomic_load_n(&obj_, __ATOMIC_ACQUIRE);
    expected.pstamp = expectedPstamp;
    desired = expected;
    desired.pstamp = desiredPstamp;
    if (__atomic_compare_exchange_n(&obj_, &expected.obj_, desired.obj_, false,
                                    __ATOMIC_ACQ_REL, __ATOMIC_RELAXED))
      return true;
    else
      return false;
  }

  uint32_t atomicLoadPstamp() {
    Psstamp expected;
    expected.obj_ = __atomic_load_n(&obj_, __ATOMIC_ACQUIRE);
    return expected.pstamp;
  }

  uint32_t atomicLoadSstamp() {
    Psstamp expected;
    expected.obj_ = __atomic_load_n(&obj_, __ATOMIC_ACQUIRE);
    return expected.sstamp;
  }

  void atomicStorePstamp(uint32_t newpstamp) {
    Psstamp expected, desired;
    expected.obj_ = __atomic_load_n(&obj_, __ATOMIC_ACQUIRE);
    for (;;) {
      desired = expected;
      desired.pstamp = newpstamp;
      if (__atomic_compare_exchange_n(&obj_, &expected.obj_, desired.obj_,
                                      false, __ATOMIC_ACQ_REL,
                                      __ATOMIC_ACQUIRE))
        break;
    }
  }

  void atomicStoreSstamp(uint32_t newsstamp) {
    Psstamp expected, desired;
    expected.obj_ = __atomic_load_n(&obj_, __ATOMIC_ACQUIRE);
    for (;;) {
      desired = expected;
      desired.sstamp = newsstamp;
      if (__atomic_compare_exchange_n(&obj_, &expected.obj_, desired.obj_,
                                      false, __ATOMIC_ACQ_REL,
                                      __ATOMIC_ACQUIRE))
        break;
    }
  }
};

class Version {
public:
  alignas(CACHE_LINE_SIZE) Version *prev_;  // Pointer to overwritten version
  Version *committed_prev_;  // Pointer to the next committed version, to reduce
  // serach cost.
  std::atomic <uint32_t> cstamp_;  // Version creation stamp, c(V)
  std::atomic <VersionStatus> status_;
  char val_[VAL_SIZE] = {};

  Version() {
    status_.store(VersionStatus::inFlight, std::memory_order_release);
  }

  void init() {
    prev_ = nullptr;
    committed_prev_ = nullptr;
    cstamp_.store(0, std::memory_order_relaxed);
    status_.store(VersionStatus::inFlight, std::memory_order_relaxed);
  }
};
