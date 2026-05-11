#pragma once

#include "../../include/op_element.hh"

#include "txid.hh"
#include "version.hh"

template<typename T>
class ReadElement : public OpElement<T> {
public:
  using OpElement<T>::OpElement;

  TxID txid_;
  TupleId tuple_id_;
  Version *ver_;

  ReadElement(Storage s, std::string_view key, T *rcdptr, Version *ver, TxID txid, TupleId tid)
      : OpElement<T>::OpElement(s, key, rcdptr) {
    ver_ = ver;
    txid_ = txid;
    tuple_id_ = tid;
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

  Version *new_ver_;
  bool finish_version_install_;

  WriteElement(Storage s, std::string_view key, T *rcdptr, Version *new_ver, OpType op)
          : OpElement<T>::OpElement(s, key, rcdptr, op) {
    new_ver_ = new_ver;
    finish_version_install_ = false;
  }

  bool operator<(const WriteElement &right) const {
    if (this->storage_ != right.storage_) return this->storage_ < right.storage_;
    return this->key_ < right.key_;
  }
};
