#pragma once

#include <pthread.h>
#include <string.h>

#include <atomic>
#include <cstdint>

#include "../../include/cache_line_size.hh"

struct Tidword {
  union {
    uint64_t obj_;
    struct {
      bool lock: 1;
      bool latest: 1;
      bool absent: 1;
      uint64_t tid: 29;
      uint64_t epoch: 32;
    };
  };

  Tidword() : obj_(0) {};

  bool operator==(const Tidword &right) const { return obj_ == right.obj_; }

  bool operator!=(const Tidword &right) const { return !operator==(right); }

  bool operator<(const Tidword &right) const { return this->obj_ < right.obj_; }
};

class Tuple {
public:
  alignas(CACHE_LINE_SIZE) Tidword tidword_;

  char val_[VAL_SIZE];
};
