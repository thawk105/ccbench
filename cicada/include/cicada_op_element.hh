#pragma once

#include "./version.hh"

template <typename T>
class ReadElement : public OpElement<T> {
 public:
  using OpElement<T>::OpElement;

  Version *ver_;

  ReadElement(uint64_t key, T *rcdptr, Version *ver)
      : OpElement<T>::OpElement(key, rcdptr) {
    this->ver_ = ver;
  }

  bool operator<(const ReadElement &right) const {
    return this->key_ < right.key_;
  }
};

template <typename T>
class WriteElement : public OpElement<T> {
 public:
  using OpElement<T>::OpElement;

  Version* newObject_;
  bool rmw_;
  bool finish_version_install_;

  WriteElement(uint64_t key, T* rcdptr, Version* newOb, bool rmw)
      : OpElement<T>::OpElement(key, rcdptr) {
    newObject_ = newOb;
    rmw_ = rmw;
    finish_version_install_ = false;
  }

  bool operator<(const WriteElement &right) const {
    return this->key_ < right.key_;
  }
};

template <typename T>
class GCElement : public OpElement<T> {
 public:
  using OpElement<T>::OpElement;

  Version *ver_;
  uint64_t wts_;

  GCElement() : ver_(nullptr), wts_(0) { this->key_ = 0; }

  GCElement(uint64_t key, T *rcdptr, Version *ver, uint64_t wts)
      : OpElement<T>::OpElement(key, rcdptr) {
    this->ver_ = ver;
    this->wts_ = wts;
  }
};
