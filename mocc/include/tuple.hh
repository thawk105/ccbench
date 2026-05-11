#pragma once

#include <string.h>  // memcpy
#include <atomic>
#include <cstdint>

#include "lock.hh"

#include "../../include/cache_line_size.hh"
#include "../../include/tuple_body.hh"

#define TEMP_MAX 20

struct Tidword {
  union {
    uint64_t obj_;
    struct {
      bool absent: 1;
      uint64_t tid: 31;
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
  Epotemp epotemp_;
  TupleBody body_;
#ifdef RWLOCK
  ReaderWriterLock rwlock_;  // 4byte
  // size to here is 20 bytes
#endif
#ifdef MQLOCK
  MQLock mqlock_;
#endif

  Tuple() {}

  void init([[maybe_unused]] size_t thid, TupleBody&& body, [[maybe_unused]] void* p) {
    // for initializer
    tidword_.epoch = 1;
    tidword_.tid = 0;
    tidword_.absent = false;
    epotemp_.temp = 0;
    epotemp_.epoch = 1;
    body_ = std::move(body);
  }

  void init(TupleBody&& body) {
    tidword_.tid = 0;
    tidword_.absent = true;
    epotemp_.temp = 0;
    body_ = std::move(body);
  }
};
