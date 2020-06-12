#pragma once

#include "../../include/op_element.hh"

#include "transaction_table.hh"
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
    return this->key_ < right.key;
  }
};

template<typename T>
class GCElement : public OpElement<T> {
public:
  using OpElement<T>::OpElement;

  Version *ver_;
  uint32_t cstamp_;

  GCElement() : OpElement<T>::OpElement() {
    this->ver_ = nullptr;
    cstamp_ = 0;
  }

  GCElement(uint64_t key, T *rcdptr, Version *ver, uint32_t cstamp)
          : OpElement<T>::OpElement(key, rcdptr) {
    this->ver_ = ver;
    this->cstamp_ = cstamp;
  }
};

class GCTMTElement {
public:
  TransactionTable *tmt_;

  GCTMTElement() : tmt_(nullptr) {}

  GCTMTElement(TransactionTable *tmt) : tmt_(tmt) {}
};
