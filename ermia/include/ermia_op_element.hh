#pragma once

#include "../../include/op_element.hh"

#include "version.hh"

template<typename T>
class SetElement : public OpElement<T> {
public:
  using OpElement<T>::OpElement;

  Version *ver_;

  SetElement(uint64_t key, T *rcdptr, Version *ver)
          : OpElement<T>::OpElement(key, rcdptr) {
    this->ver_ = ver;
  }

  bool operator<(const SetElement &right) const {
    return this->key_ < right.key_;
  }
};

template<typename T>
class GCElement : public OpElement<T> {
public:
  using OpElement<T>::OpElement;

  Version *ver_;
  uint32_t cstamp_;

  GCElement(uint64_t key, T *rcdptr, Version *ver, uint32_t cstamp)
          : OpElement<T>::OpElement(key, rcdptr) {
    this->ver_ = ver;
    this->cstamp_ = cstamp;
  }
};
