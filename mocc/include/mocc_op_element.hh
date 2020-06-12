#pragma once

#include <iostream>

#include "../../include/op_element.hh"

using std::cout;
using std::endl;

template<typename T>
class ReadElement : public OpElement<T> {
public:
  using OpElement<T>::OpElement;

  Tidword tidword_;
  char val_[VAL_SIZE];
  bool failed_verification_;

  ReadElement(Tidword tidword, uint64_t key, T *rcdptr, char *newVal)
          : OpElement<T>::OpElement(key, rcdptr) {
    this->tidword_ = tidword;
    memcpy(val_, newVal, VAL_SIZE);
    this->failed_verification_ = false;
  }

  ReadElement(uint64_t key, T *rcdptr) : OpElement<T>::OpElement(key, rcdptr) {
    failed_verification_ = true;
  }

  bool operator<(const ReadElement &right) const {
    return this->key_ < right.key_;
  }
};

template<typename T>
class WriteElement : public OpElement<T> {
public:
  using OpElement<T>::OpElement;

  bool operator<(const WriteElement &right) const {
    return this->key_ < right.key_;
  }
};
