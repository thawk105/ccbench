#pragma once

#include <atomic>
#include <mutex>

#include "../../include/cache_line_size.hh"
#include "../../include/inline.hh"
#include "../../include/rwlock.hh"
#include "../../include/tuple_body.hh"

using namespace std;

class Tuple {
  public:
  alignas(CACHE_LINE_SIZE) ReaderWriteLock lock_;
  TupleBody body_;

  Tuple() {}

  void init([[maybe_unused]] size_t thid, TupleBody&& body, [[maybe_unused]] void* p) {
    body_ = std::move(body);
  }

  void init(TupleBody&& body) {
    body_ = std::move(body);
    lock_.w_lock();
  }
};
