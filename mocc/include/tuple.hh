#pragma once

#include <string.h>  // memcpy
#include <atomic>
#include <cstdint>

#include "lock.hh"

#include "../../include/cache_line_size.hh"

#define TEMP_THRESHOLD 5
#define TEMP_MAX 20
#define TEMP_RESET_US 100

struct Tidword {
  union {
    uint64_t obj_;
    struct {
      uint64_t tid: 32;
      uint64_t epoch: 32;
    };
  };

  Tidword() { obj_ = 0; }

  bool operator==(const Tidword &right) const { return obj_ == right.obj_; }

  bool operator!=(const Tidword &right) const { return !operator==(right); }

  bool operator<(const Tidword &right) const { return this->obj_ < right.obj_; }
};

// 32bit temprature, 32bit epoch
struct Epotemp {
  union {
    alignas(CACHE_LINE_SIZE) uint64_t obj_;
    struct {
      uint64_t temp: 32;
      uint64_t epoch: 32;
    };
  };

  Epotemp() : obj_(0) {}

  Epotemp(uint64_t temp2, uint64_t epoch2) : temp(temp2), epoch(epoch2) {}

  bool operator==(const Epotemp &right) const { return obj_ == right.obj_; }

  bool operator!=(const Epotemp &right) const { return !operator==(right); }

  bool eqEpoch(uint64_t epo) {
    if (epoch == epo)
      return true;
    else
      return false;
  }
};

class Tuple {
public:
  alignas(CACHE_LINE_SIZE) Tidword tidword_;
#ifdef RWLOCK
  RWLock rwlock_;  // 4byte
  // size to here is 20 bytes
#endif
#ifdef MQLOCK
  MQLock mqlock_;
#endif

  char val_[VAL_SIZE];
};
