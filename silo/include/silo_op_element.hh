#pragma once

#include "../../include/op_element.hh"

template <typename T>
class ReadElement : public OpElement<T> {
 public:
  using OpElement<T>::OpElement;

  Tidword tidword_;
  char val_[VAL_SIZE];

  ReadElement(uint64_t key, T* rcdptr, char* val, Tidword tidword)
      : OpElement<T>::OpElement(key, rcdptr) {
    tidword_.obj_ = tidword.obj_;
    memcpy(this->val_, val, VAL_SIZE);
  }

  bool operator<(const ReadElement& right) const {
    return this->key_ < right.key_;
  }
};

template <typename T>
class WriteElement : public OpElement<T> {
 public:
  using OpElement<T>::OpElement;

  WriteElement(uint64_t key, T* rcdptr)
      : OpElement<T>::OpElement(key, rcdptr) {}

  bool operator<(const WriteElement& right) const {
    return this->key_ < right.key_;
  }
};
