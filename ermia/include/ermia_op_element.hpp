#pragma once

#include "../../include/op_element.hpp"

#include "version.hpp"

template <typename T>
class SetElement : public Op_element<T> {
public:
  using Op_element<T>::Op_element;

  Version *ver_;

  SetElement(uint64_t key, T *rcdptr, Version *ver) : Op_element<T>::Op_element(key, rcdptr) {
    this->ver_ = ver;
  }

  bool operator<(const SetElement& right) const {
    return this->key_ < right.key_;
  }
};

template <typename T>
class GCElement : public Op_element<T> {
public:
  using Op_element<T>::Op_element;

  Version *ver_;
  uint32_t cstamp_;

  GCElement(uint64_t key, T *rcdptr, Version *ver, uint32_t cstamp) : Op_element<T>::Op_element(key, rcdptr) {
    this->ver_ = ver;
    this->cstamp_ = cstamp;
  }
};

