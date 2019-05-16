#pragma once

#include <atomic>
#include <cstdint>

#include "../../include/cache_line_size.hpp"

enum class VersionStatus : uint8_t {
  inFlight,
  committed,
  aborted,
};

struct Psstamp {
  union {
    uint64_t obj;
    struct {
      uint32_t pstamp;
      uint32_t sstamp;
    };
  };

  Psstamp() {
    obj = 0;
  }

  void init(uint32_t pstamp, uint32_t sstamp) {
    this->pstamp = pstamp;
    this->sstamp = sstamp;
  }

  uint64_t atomicLoad() {
    Psstamp expected;
    expected.obj = __atomic_load_n(&obj, __ATOMIC_ACQUIRE);
    return expected.obj;
  }

  bool atomicCASPstamp(uint32_t expectedPstamp, uint32_t desiredPstamp) {
    Psstamp expected, desired;
    expected.obj = __atomic_load_n(&obj, __ATOMIC_ACQUIRE);
    expected.pstamp = expectedPstamp;
    desired = expected;
    desired.pstamp = desiredPstamp;
    if (__atomic_compare_exchange_n(&obj, &expected.obj, desired.obj, false, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED)) return true;
    else return false;
  }

  uint32_t atomicLoadPstamp() {
    Psstamp expected;
    expected.obj = __atomic_load_n(&obj, __ATOMIC_ACQUIRE);
    return expected.pstamp;
  }

  uint32_t atomicLoadSstamp() {
    Psstamp expected;
    expected.obj = __atomic_load_n(&obj, __ATOMIC_ACQUIRE);
    return expected.sstamp;
  }
  void atomicStorePstamp(uint32_t newpstamp) {
    Psstamp expected, desired;
    expected.obj = __atomic_load_n(&obj, __ATOMIC_ACQUIRE);
    for (;;) {
      desired = expected;
      desired.pstamp = newpstamp;
      if (__atomic_compare_exchange_n(&obj, &expected.obj, desired.obj, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) break;
    }
  }

  void atomicStoreSstamp(uint32_t newsstamp) {
    Psstamp expected, desired;
    expected.obj = __atomic_load_n(&obj, __ATOMIC_ACQUIRE);
    for (;;) {
      desired = expected;
      desired.sstamp = newsstamp;
      if (__atomic_compare_exchange_n(&obj, &expected.obj, desired.obj, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) break;
    }
  }
};

class Version {
public:
  Version *prev;  // Pointer to overwritten version
  Version *committed_prev;  // Pointer to the next committed version, to reduce serach cost.

  std::atomic<uint32_t> cstamp;       // Version creation stamp, c(V)
  std::atomic<VersionStatus> status;
  // size to here is 21 bytes.

  char val[VAL_SIZE] = {};

  int8_t pad[CACHE_LINE_SIZE - ((21 + VAL_SIZE) % CACHE_LINE_SIZE)];

  Version() {
    status.store(VersionStatus::inFlight, std::memory_order_release);
  }
};

class SetElement {
public:
  unsigned int key;
  Version *ver;

  SetElement(unsigned int key, Version *ver) {
    this->key = key;
    this->ver = ver;
  }

  bool operator<(const SetElement& right) const {
    return this->key < right.key;
  }
};

