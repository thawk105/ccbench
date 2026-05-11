#pragma once

#include "../../include/op_element.hh"

#include "version.hh"

template<typename T>
class ReadElement : public OpElement<T> {
public:
  using OpElement<T>::OpElement;

  Version *later_ver_, *ver_;

  ReadElement(Storage s, std::string_view key, T *rcdptr, Version *later_ver, Version *ver)
          : OpElement<T>::OpElement(s, key, rcdptr) {
    later_ver_ = later_ver;
    ver_ = ver;
  }

  bool operator<(const ReadElement &right) const {
    if (this->storage_ != right.storage_) return this->storage_ < right.storage_;
    return this->key_ < right.key_;
  }
};

template<typename T>
class WriteElement : public OpElement<T> {
public:
  using OpElement<T>::OpElement;

  Version *later_ver_, *new_ver_;
  bool finish_version_install_;

  WriteElement(Storage s, std::string_view key, T *rcdptr, Version *later_ver, Version *new_ver, OpType op)
          : OpElement<T>::OpElement(s, key, rcdptr, op) {
    later_ver_ = later_ver;
    new_ver_ = new_ver;
    finish_version_install_ = false;
  }

  WriteElement(Storage s, std::string_view key, T *rcdptr, Version *new_ver, OpType op)
          : OpElement<T>::OpElement(s, key, rcdptr, op) {
    // for insert
    later_ver_ = nullptr;
    new_ver_ = new_ver;
    finish_version_install_ = true;
  }

  bool operator<(const WriteElement &right) const {
    if (this->storage_ != right.storage_) return this->storage_ < right.storage_;
    return this->key_ < right.key_;
  }
};

template<typename T>
class GCElement : public OpElement<T> {
public:
  using OpElement<T>::OpElement;

  Version *ver_;
  uint64_t wts_;

  GCElement() : ver_(nullptr), wts_(0) { this->s; this->key_ = ""; }

  GCElement(Storage s, std::string_view key, T *rcdptr, Version *ver, uint64_t wts)
          : OpElement<T>::OpElement(s, key, rcdptr) {
    this->ver_ = ver;
    this->wts_ = wts;
  }
};
