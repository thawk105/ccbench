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
  atomic<uint64_t> rts;
  atomic<uint64_t> wts;
  atomic<Version *> next;
  atomic<VersionStatus> status; //commit record
  // version size to here is 25 bytes.

  char val[VAL_SIZE];

  int8_t pad[CACHE_LINE_SIZE - ((25 + VAL_SIZE) % (CACHE_LINE_SIZE))];
  // it is description for convenience.
  // 25 : rts(8) + wts(8) + next(8) + status(1)

  Version() {
    status.store(VersionStatus::pending, memory_order_release);
    next.store(nullptr, memory_order_release);
  }

  Version(uint64_t rts, uint64_t wts) {
    this->rts.store(rts, memory_order_relaxed);
    this->wts.store(wts, memory_order_relaxed);
  }

  void set(uint64_t rts, uint64_t wts) {
    this->rts.store(rts, memory_order_relaxed);
    this->wts.store(wts, memory_order_relaxed);
  }

  void set(uint64_t rts, uint64_t wts, Version *newnex, VersionStatus newst) {
    this->rts.store(rts, memory_order_relaxed);
    this->wts.store(wts, memory_order_relaxed);
    next = newnex;
    status = newst;
  }

  uint64_t ldAcqRts() {
    return rts.load(std::memory_order_acquire);
  }

  uint64_t ldAcqWts() {
    return wts.load(std::memory_order_acquire);
  }

  Version *ldAcqNext() {
    return next.load(std::memory_order_acquire);
  }

  VersionStatus ldAcqStatus() {
    return status.load(std::memory_order_acquire);
  }

  void strRelNext(Version *next_) {
    next.store(next_, std::memory_order_release);
  }
};

