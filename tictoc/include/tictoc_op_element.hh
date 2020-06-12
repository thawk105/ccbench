#pragma once

#include "../../include/op_element.hh"

template<typename T>
class SetElement : public OpElement<T> {
public:
  using OpElement<T>::OpElement;

  char val_[VAL_SIZE];
  TsWord tsw_;

  SetElement(uint64_t key, T *rcdptr, char *val, TsWord tsw)
          : OpElement<T>::OpElement(key, rcdptr) {
    memcpy(this->val_, val, VAL_SIZE);
    this->tsw_.obj_ = tsw.obj_;
  }

  SetElement(uint64_t key, T *rcdptr, TsWord tsw)
          : OpElement<T>::OpElement(key, rcdptr) {
    this->tsw_.obj_ = tsw.obj_;
  }

  bool operator<(const SetElement &right) const {
    return this->key_ < right.key_;
  }
};
