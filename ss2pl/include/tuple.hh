#pragma once

#include <atomic>
#include <mutex>

#include "../../include/cache_line_size.hh"
#include "../../include/inline.hh"
#include "../../include/rwlock.hh"

using namespace std;
#define UNLOCKED INT32_MAX

class Tuple {
public:
  alignas(CACHE_LINE_SIZE) RWLock lock_;
  RWLock latch_;
  char val_[VAL_SIZE];
  int writer_;
  int reader_;
  bool writeflag_;
};
