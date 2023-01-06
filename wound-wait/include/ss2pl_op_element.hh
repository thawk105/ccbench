#pragma once

#include "../../include/op_element.hh"

template<typename T>
class SetElement : public OpElement<T> {
public:
  using OpElement<T>::OpElement;

  char val_[VAL_SIZE];

  SetElement(uint64_t key, T *rcdptr) : OpElement<T>::OpElement(key, rcdptr) {}

  SetElement(uint64_t key, T *rcdptr, char *val)
          : OpElement<T>::OpElement(key, rcdptr) {
    memcpy(this->val_, val, VAL_SIZE);
  }
};
