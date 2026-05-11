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
  TupleBody body_;
  bool failed_verification_;

  ReadElement(Storage s, std::string_view key, T *rcdptr, TupleBody&& body, Tidword tidword)
          : OpElement<T>::OpElement(s, key, rcdptr), body_(std::move(body)) {
    this->tidword_ = tidword;
    this->failed_verification_ = false;
  }

  ReadElement(uint64_t key, T *rcdptr) : OpElement<T>::OpElement(key, rcdptr) {
    failed_verification_ = true;
  }

  bool operator<(const ReadElement &right) const {
    return this->rcdptr_ < right.rcdptr_;
  }
};

template<typename T>
class WriteElement : public OpElement<T> {
public:
  using OpElement<T>::OpElement;
  TupleBody body_;

  WriteElement(Storage s, std::string_view key, T* rcdptr, TupleBody&& body, OpType op)
          : OpElement<T>::OpElement(s, key, rcdptr, op), body_(std::move(body)) {
  }

  WriteElement(Storage s, std::string_view key, T* rcdptr, OpType op)
          : OpElement<T>::OpElement(s, key, rcdptr, op) {
  }

  bool operator<(const WriteElement &right) const {
    return this->rcdptr_ < right.rcdptr_;
  }
};
