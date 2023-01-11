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
  atomic<int> curr_writer = 0;
  std::vector<std::pair<int, int>> waitRd;
  std::vector<std::pair<int, int>> waitWr;
  Tuple() {
    waitRd.reserve(224);
    waitWr.reserve(224);
  }
};
