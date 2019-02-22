#pragma once

#include <string.h> // memcpy
#include <sys/time.h>

#include <atomic>
#include <cstdint>
#include "timeStamp.hpp"

using namespace std;

enum class VersionStatus : uint8_t {
  invalid,
  pending,
  aborted,
  precommitted, //early lock releaseで利用
  committed,
  deleted,
};

class Version {
public:
  atomic<uint64_t> rts;
  atomic<uint64_t> wts;
  atomic<Version *> next;
  atomic<VersionStatus> status; //commit record
  int8_t pad[3];
  // version size to here is 28 bytes.

  char val[VAL_SIZE];

  Version() {
    status.store(VersionStatus::pending, memory_order_release);
    next.store(nullptr, memory_order_release);
  }

  Version(uint64_t rts, uint64_t wts) {
    this->rts.store(rts, memory_order_relaxed);
    this->wts.store(wts, memory_order_relaxed);
  }
};

class ElementSet {
public:
  Version *sourceObject = nullptr;
  Version *newObject = nullptr;
};

class ReadElement {
public:
  unsigned int key;
  Version *ver;

  ReadElement(unsigned int key, Version *ver) {
    this->key = key;
    this->ver = ver;
  }

  bool operator<(const ReadElement& right) const {
    return this->key < right.key;
  }
};

class WriteElement {
public:
  unsigned int key;
  Version *sourceObject, *newObject;

  WriteElement(unsigned int key, Version *source, Version *newOb) {
    this->key = key;
    this->sourceObject = source;
    this->newObject = newOb;
  }

  bool operator<(const WriteElement& right) const {
    return this->key < right.key;
  }
};

class GCElement {
public:
  unsigned int key;
  Version *ver;
  uint64_t wts;

  GCElement(unsigned int key, Version *ver, uint64_t wts) {
    this->key = key;
    this->ver = ver;
    this->wts = wts;
  }
};
