#pragma once

#include "../../include/op_element.hpp"

template <typename T>
class SetElement : public Op_element<T> {
public:
  using Op_element<T>::Op_element;

  char val[VAL_SIZE];
  TsWord tsw;

  SetElement(uint64_t key, T *rcdptr, char* newval, TsWord tsw) : Op_element<T>::Op_element(key, rcdptr) {
    memcpy(this->val, newval, VAL_SIZE);
    this->tsw.obj = tsw.obj;
  }

  SetElement(uint64_t key, T *rcdptr, TsWord tsw) : Op_element<T>::Op_element(key, rcdptr) {
    this->tsw.obj = tsw.obj;
  }

  bool operator<(const SetElement& right) const {
    return this->key < right.key;
  }
};

