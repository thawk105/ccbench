#pragma once

#include <queue>

#include "cache_line_size.hh"
#include "debug.hh"
#include "lock.hh"
#include "util.hh"

template<typename T>
class ConcurrentQueue : std::queue<T> {
  typedef std::queue<T> super;
  alignas(CACHE_LINE_SIZE) RWLock lock_;

public:
  void push(T element) {
    lock_.w_lock();
    super::push(element);
    lock_.w_unlock();
  }

  T pop() {
    T element;
    lock_.w_lock();
    if (super::empty()) {
      lock_.w_unlock();
      throw std::out_of_range("no element in queue");
    } else {
      element = super::front();
      super::pop();
      lock_.w_unlock();
    }
    return element;
  }

  size_t size() {
    return super::size();
  }
};
