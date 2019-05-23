#pragma once

#include "../../include/op_element.hpp"

#include "transaction.hpp"
#include "version.hpp"

template <typename T>
class SetElement : public Op_element<T> {
public:
  Version *ver;

  SetElement(uint64_t key, T *rcdptr, Version *ver) {
    this->key = key;
    this->rcdptr = rcdptr;
    this->ver = ver;
  }

  bool operator<(const SetElement& right) const {
    return this->key < right.key;
  }
};

template <typename T>
class GCElement : public Op_element<T> {
public:
  Version *ver;
  uint32_t cstamp;

  GCElement() : ver(nullptr), cstamp(0) {}

  GCElement(uint64_t key, T *rcdptr, Version *ver, uint32_t cstamp) {
    this->key = key;
    this->rcdptr = rcdptr;
    this->ver = ver;
    this->cstamp = cstamp;
  }
};

// forward declaration.
class TransactionTable;

class GCTMTElement {
public:
  TransactionTable *tmt;

  GCTMTElement() : tmt(nullptr) {}
  GCTMTElement(TransactionTable *tmt_) : tmt(tmt_) {}
};
