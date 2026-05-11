#pragma once

#include "../../include/op_element.hh"

#include "version.hh"

template<typename T>
class SetElement : public OpElement<T> {
public:
  using OpElement<T>::OpElement;

  Version *ver_;

  SetElement(Storage s, std::string_view key, T *rcdptr, Version *ver)
          : OpElement<T>::OpElement(s, key, rcdptr) {
    this->ver_ = ver;
  }

  SetElement(Storage s, std::string_view key, T *rcdptr, Version *ver, OpType op)
          : OpElement<T>::OpElement(s, key, rcdptr, op) {
    this->ver_ = ver;
  }

  bool operator<(const SetElement &right) const {
    if (this->storage_ != right.storage_) return this->storage_ < right.storage_;
    return this->key_ < right.key_;
  }
};

template<typename T>
class GCElement : public OpElement<T> {
public:
  using OpElement<T>::OpElement;

  Version *ver_;
  uint32_t cstamp_;

  GCElement(Storage s, std::string_view key, T *rcdptr, Version *ver, uint32_t cstamp)
          : OpElement<T>::OpElement(s, key, rcdptr) {
    this->ver_ = ver;
    this->cstamp_ = cstamp;
  }
};
