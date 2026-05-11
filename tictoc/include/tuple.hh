#pragma once

#include <pthread.h>
#include <string.h>

#include <atomic>
#include <cstdint>

#include "../../include/cache_line_size.hh"
#include "../../include/tuple_body.hh"

#pragma pack(push, 1)
struct TsWord {
  union {
    uint64_t obj_;
    struct {
      bool lock: 1;
      bool absent: 1;
      uint16_t delta: 15;
      uint64_t wts: 47;
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
#pragma pack(pop)

class Tuple {
public:
  alignas(CACHE_LINE_SIZE) TsWord tsw_;
  TsWord pre_tsw_;
  TupleBody body_;

  Tuple() {}

  void init([[maybe_unused]] size_t thid, TupleBody&& body, [[maybe_unused]] void* p) {
    // for initializer
    tsw_.obj_ = 0;
    pre_tsw_.obj_ = 0;
    body_ = std::move(body);
  }

  void init(TupleBody&& body) {
    tsw_.lock = true;
    tsw_.absent = true;
    tsw_.wts = 0;
    tsw_.delta = 0;
    pre_tsw_.obj_ = 0;
    body_ = std::move(body);
  }
};
