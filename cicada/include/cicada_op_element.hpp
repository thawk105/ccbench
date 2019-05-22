#pragma once

#include "./version.hpp"

template <typename T>
class ReadElement : public Op_element<T> {
public:
  Version *ver;

  ReadElement(uint64_t key, T *rcdptr, Version *ver) {
    this->key = key;
    this->rcdptr = rcdptr;
    this->ver = ver;
  }

  bool operator<(const ReadElement& right) const {
    return this->key < right.key;
  }
};

template <typename T>
class WriteElement : public Op_element<T> {
public:
  Version *newObject;
  bool finishVersionInstall;

  WriteElement(uint64_t key, T *rcdptr, Version *newOb) {
    this->key = key;
    this->rcdptr = rcdptr;
    this->newObject = newOb;
    finishVersionInstall = false;
  }

  bool operator<(const WriteElement& right) const {
    return this->key < right.key;
  }
};

template <typename T>
class GCElement : public Op_element<T> {
public:
  Version *ver;
  uint64_t wts;

  GCElement() : ver(nullptr), wts(0) {
    this->key = 0;
  }

  GCElement(uint64_t key, T *rcdptr, Version *ver, uint64_t wts) {
    this->key = key;
    this->rcdptr = rcdptr;
    this->ver = ver;
    this->wts = wts;
  }
};
