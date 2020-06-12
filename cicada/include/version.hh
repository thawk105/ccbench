#pragma once

#include <stdio.h>
#include <string.h>  // memcpy
#include <sys/time.h>

#include <atomic>
#include <cstdint>

#include "../../include/cache_line_size.hh"
#include "../../include/op_element.hh"

using namespace std;

enum class VersionStatus : uint8_t {
  invalid,
  pending,
  aborted,
  precommitted,  // now, unuse.
  committed,
  deleted,
  unused,
};

class Version {
public:
  alignas(CACHE_LINE_SIZE) atomic <uint64_t> rts_;
  atomic <uint64_t> wts_;
  atomic<Version *> next_;
  atomic <VersionStatus> status_;  // commit record

  char val_[VAL_SIZE];

  Version() {
    status_.store(VersionStatus::pending, memory_order_release);
    next_.store(nullptr, memory_order_release);
  }

  Version(const uint64_t rts, const uint64_t wts) {
    rts_.store(rts, memory_order_relaxed);
    wts_.store(wts, memory_order_relaxed);
    status_.store(VersionStatus::pending, memory_order_release);
    next_.store(nullptr, memory_order_release);
  }

  void displayInfo() {
    printf(
            "Version::displayInfo(): this: %p rts_: %lu: wts_: %lu: next_: %p: "
            "status_: "
            "%u\n",
            this, ldAcqRts(), ldAcqWts(), ldAcqNext(), (uint8_t) ldAcqStatus());
  }

  Version *latestCommittedVersionAfterThis() {
    Version *version = this;
    while (version->ldAcqStatus() != VersionStatus::committed)
      version = version->ldAcqNext();
    return version;
  }

  Version *ldAcqNext() { return next_.load(std::memory_order_acquire); }

  uint64_t ldAcqRts() { return rts_.load(std::memory_order_acquire); }

  VersionStatus ldAcqStatus() {
    return status_.load(std::memory_order_acquire);
  }

  uint64_t ldAcqWts() { return wts_.load(std::memory_order_acquire); }

  void set(const uint64_t rts, const uint64_t wts) {
    rts_.store(rts, memory_order_relaxed);
    wts_.store(wts, memory_order_relaxed);
    status_.store(VersionStatus::pending, memory_order_release);
    next_.store(nullptr, memory_order_release);
  }

  void set(const uint64_t rts, const uint64_t wts, Version *next,
           const VersionStatus status) {
    rts_.store(rts, memory_order_relaxed);
    wts_.store(wts, memory_order_relaxed);
    status_.store(status, memory_order_release);
    next_.store(next, memory_order_release);
  }

  Version *skipTheStatusVersionAfterThis(const VersionStatus status,
                                         const bool pendingWait) {
    Version *ver = this;
    VersionStatus local_status = ver->ldAcqStatus();
    if (pendingWait)
      while (local_status == VersionStatus::pending) {
        local_status = ver->ldAcqStatus();
      }
    while (local_status == status) {
      ver = ver->ldAcqNext();
      local_status = ver->ldAcqStatus();
      if (pendingWait)
        while (local_status == VersionStatus::pending) {
          local_status = ver->ldAcqStatus();
        }
    }
    return ver;
  }

  Version *skipNotTheStatusVersionAfterThis(const VersionStatus status,
                                            const bool pendingWait) {
    Version *ver = this;
    if (pendingWait)
      while (ver->ldAcqStatus() == VersionStatus::pending);
    while (ver->ldAcqStatus() != status) {
      ver = ver->ldAcqNext();
      if (pendingWait)
        while (ver->ldAcqStatus() == VersionStatus::pending);
    }
    return ver;
  }

  void strRelNext(Version *next) {  // store release next = strRelNext
    next_.store(next, std::memory_order_release);
  }
};
