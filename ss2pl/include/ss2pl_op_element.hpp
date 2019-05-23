#pragma once

#include "../../include/op_element.hpp"

template <typename T>
class SetElement : public Op_element<T> {
public:
  using Op_element<T>::Op_element;

  char val[VAL_SIZE];

  SetElement(uint64_t key, T *rcdptr) : Op_element<T>::Op_element(key, rcdptr) {}
    
  SetElement(uint64_t key, T *rcdptr, char *val) : Op_element<T>::Op_element(key, rcdptr) {
    memcpy(this->val, val, VAL_SIZE);
  }
};
