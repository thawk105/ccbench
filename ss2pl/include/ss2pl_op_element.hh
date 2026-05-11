#pragma once

#include "../../include/op_element.hh"

template<typename T>
class SetElement : public OpElement<T> {
  public:
  using OpElement<T>::OpElement;
  TupleBody body_;

  SetElement(Storage s, std::string_view key, T *rcdptr, TupleBody&& body)
      : OpElement<T>::OpElement(s, key, rcdptr), body_(std::move(body)) {}

  SetElement(Storage s, std::string_view key, T *rcdptr)
      : OpElement<T>::OpElement(s, key, rcdptr) {}

  SetElement(Storage s, std::string_view key, T *rcdptr, OpType op)
      : OpElement<T>::OpElement(s, key, rcdptr, op) {}

  SetElement(Storage s, std::string_view key, T *rcdptr, TupleBody&& body, OpType op)
      : OpElement<T>::OpElement(s, key, rcdptr, op), body_(std::move(body)) {}

  bool operator<(const SetElement &right) const {
    if (this->storage_ != right.storage_) return this->storage_ < right.storage_;
    return this->key_ < right.key_;
  }
};
