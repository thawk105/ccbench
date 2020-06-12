#pragma once

#include <pthread.h>
#include <string.h>

#include <atomic>
#include <cstdint>

#include "../../include/cache_line_size.hh"

struct TsWord {
  union {
    uint64_t obj_;
    struct {
      bool lock: 1;
      uint16_t delta: 15;
      uint64_t wts: 48;
    };
  };

  TsWord() { obj_ = 0; }

  bool operator==(const TsWord &right) const { return obj_ == right.obj_; }

  bool operator!=(const TsWord &right) const { return !operator==(right); }

  bool isLocked() {
    if (lock)
      return true;
    else
      return false;
  }

  uint64_t rts() const { return wts + delta; }
};

class Tuple {
public:
  alignas(CACHE_LINE_SIZE) TsWord tsw_;
  TsWord pre_tsw_;
  char val_[VAL_SIZE];
};
