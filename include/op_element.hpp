#pragma once

#include "debug.hpp"

template <typename T>
class Op_element {
public:
  uint64_t key;
  T* rcdptr;

  Op_element() : key(0), rcdptr(nullptr) {}
  Op_element(uint64_t key_) : key(key_) {}
  Op_element(uint64_t key_, T* rcdptr_) : key(key_), rcdptr(rcdptr_) {}
};

