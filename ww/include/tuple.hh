#pragma once

#include <atomic>
#include <mutex>

#include "../../include/cache_line_size.hh"
#include "../../include/inline.hh"
#include "../../include/rwlock.hh"

using namespace std;

class Tuple {
public:
  alignas(CACHE_LINE_SIZE) RWLock lock_;
  char val_[VAL_SIZE];
  int writer = 0;
  int readers[224] = {0};
  int writers[224] = {0};
  std::vector<int> waitRd;
  std::vector<int> waitWR;
  Tuple() {
    waitRd.reserve(224);
    waitWR.reserve(224);
  }
};
