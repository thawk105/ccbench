#pragma once

#include "debug.hh"

template<typename T>
class OpElement {
public:
  uint64_t key_;
  T *rcdptr_;

  OpElement() : key_(0), rcdptr_(nullptr) {}

  OpElement(uint64_t key) : key_(key) {}

  OpElement(uint64_t key, T *rcdptr) : key_(key), rcdptr_(rcdptr) {}
};
