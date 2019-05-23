#pragma once

#include "../../include/op_element.hpp"

template <typename T>
class ReadElement : public Op_element<T> {
public:
  using Op_element<T>::Op_element;

  Tidword tidword;
  char val[VAL_SIZE];

  ReadElement(uint64_t key, T *rcdptr, char* newval, Tidword newtidword) : Op_element<T>::Op_element(key, rcdptr) {
    tidword.obj = newtidword.obj;
    memcpy(this->val, newval, VAL_SIZE);
  }

  bool operator<(const ReadElement& right) const {
    return this->key < right.key;
  }
};

template <typename T>
class WriteElement : public Op_element<T> {
public:
  using Op_element<T>::Op_element;

  WriteElement(uint64_t key, T* rcdptr) : Op_element<T>::Op_element(key, rcdptr) {}

  bool operator<(const WriteElement& right) const {
    return this->key < right.key;
  }
};
