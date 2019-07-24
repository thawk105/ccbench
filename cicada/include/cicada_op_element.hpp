#pragma once

#include "./version.hpp"

template <typename T>
class ReadElement : public Op_element<T> {
public:
  using Op_element<T>::Op_element;

  Version *ver_;

  ReadElement(uint64_t key, T *rcdptr, Version *ver) : Op_element<T>::Op_element(key, rcdptr) {
    this->ver_ = ver;
  }

  bool operator<(const ReadElement& right) const {
    return this->key_ < right.key_;
  }
};

template <typename T>
class WriteElement : public Op_element<T> {
public:
  using Op_element<T>::Op_element;

  Version *newObject_;
  bool finish_version_install_;

  WriteElement(uint64_t key, T *rcdptr, Version *newOb) : Op_element<T>::Op_element(key, rcdptr) {
    this->newObject_ = newOb;
    finish_version_install_ = false;
  }

  bool operator<(const WriteElement& right) const {
    return this->key_ < right.key_;
  }
};

template <typename T>
class GCElement : public Op_element<T> {
public:
  using Op_element<T>::Op_element;

  Version *ver_;
  uint64_t wts_;

  GCElement() : ver_(nullptr), wts_(0) {
    this->key_ = 0;
  }

  GCElement(uint64_t key, T *rcdptr, Version *ver, uint64_t wts) : Op_element<T>::Op_element(key, rcdptr) {
    this->ver_ = ver;
    this->wts_ = wts;
  }
};
