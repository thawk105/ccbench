#pragma once

#include "../../include/op_element.hh"

template<typename T>
class SetElement : public OpElement<T> {
public:
  using OpElement<T>::OpElement;

  TupleBody body_;
  TsWord tsw_;

  SetElement(Storage s, std::string_view key, T *rcdptr, TupleBody&& body, TsWord tsw, OpType op)
          : OpElement<T>::OpElement(s, key, rcdptr, op), body_(std::move(body)) {
    this->tsw_.obj_ = tsw.obj_;
  }

  SetElement(Storage s, std::string_view key, T *rcdptr, TupleBody&& body, TsWord tsw)
          : OpElement<T>::OpElement(s, key, rcdptr), body_(std::move(body)) {
    this->tsw_.obj_ = tsw.obj_;
  }

  SetElement(Storage s, std::string_view key, T *rcdptr, TsWord tsw, OpType op)
          : OpElement<T>::OpElement(s, key, rcdptr, op) {
    this->tsw_.obj_ = tsw.obj_;
  }

  bool operator<(const SetElement &right) const {
    if (this->storage_ != right.storage_) return this->storage_ < right.storage_;
    return this->key_ < right.key_;
  }
};

template<typename T>
class GCElement : public OpElement<T> {
public:
  using OpElement<T>::OpElement;
  uint64_t epoch_;

  GCElement(Storage s, std::string_view key, T *rcdptr, uint64_t epoch)
          : OpElement<T>::OpElement(s, key, rcdptr) {
    this->epoch_ = epoch;
  }
};
