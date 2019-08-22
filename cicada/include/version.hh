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
  alignas(CACHE_LINE_SIZE) atomic<uint64_t> rts_;
  atomic<uint64_t> wts_;
  atomic<Version *> next_;
  atomic<VersionStatus> status_;  // commit record

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
    printf("Version::displayInfo(): rts_: %lu: wts_: %lu: next_: %p: status_: %u\n", ldAcqRts(), ldAcqWts(), ldAcqNext(), (uint8_t)ldAcqStatus());
  }

  Version* latestCommittedVersionAfterThis() {
    Version* version = this;
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

  void set(const uint64_t rts, const uint64_t wts, Version *next, const VersionStatus status) {
    rts_.store(rts, memory_order_relaxed);
    wts_.store(wts, memory_order_relaxed);
    status_.store(status, memory_order_release);
    next_.store(next, memory_order_release);
  }

  Version* skipTheStatusVersionAfterThis(const VersionStatus status, const bool pendingWait) {
    Version* version = this;
    if (pendingWait) while(version->ldAcqStatus() == VersionStatus::pending);
    while (version->ldAcqStatus() == status) {
      version = version->ldAcqNext();
      if (pendingWait) while(version->ldAcqStatus() == VersionStatus::pending);
    }
    return version;
  }

  Version* skipNotTheStatusVersionAfterThis(const VersionStatus status, const bool pendingWait) {
    Version* version = this;
    if (pendingWait) while(version->ldAcqStatus() == VersionStatus::pending) {
      //printf("Th#%u: wait: %lu\n", thid, (version->ldAcqWts() & UINT8_MAX));
      //printf("version: %p, next: %p\n", version, version->ldAcqNext());
      //if (version == version->ldAcqNext()) ERR;
    }
    while (version->ldAcqStatus() != status) {
      version = version->ldAcqNext();
      if (pendingWait) while(version->ldAcqStatus() == VersionStatus::pending) {
        //printf("Th#%u: wait: %lu\n", thid, (version->ldAcqWts() & UINT8_MAX));
        //printf("version: %p, next: %p\n", version, version->ldAcqNext());
        //if(version == version->ldAcqNext()) ERR;
      }
    }
    return version;
  }

  void strRelNext(Version *next) {  // store release next = strRelNext
    next_.store(next, std::memory_order_release);
  }
};
