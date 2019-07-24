#pragma once

#include "debug.hpp"

template <typename T>
class Op_element {
public:
  uint64_t key_;
  T* rcdptr_;

  Op_element() : key_(0), rcdptr_(nullptr) {}
  Op_element(uint64_t key) : key_(key) {}
  Op_element(uint64_t key, T* rcdptr) : key_(key), rcdptr_(rcdptr) {}
};

