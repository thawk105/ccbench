#pragma once

#include <iostream>

using std::cout;
using std::endl;

enum class Ope : uint8_t {
  READ,
  WRITE,
  READ_MODIFY_WRITE,
};

class Procedure {
public:
  Ope ope_;
  uint64_t key_;
  bool ronly_ = false;
  bool wonly_ = false;

  Procedure() : ope_(Ope::READ), key_(0) {}

  Procedure(Ope ope, uint64_t key) : ope_(ope), key_(key) {}

  bool operator<(const Procedure &right) const {
    if (this->key_ == right.key_ && this->ope_ == Ope::WRITE &&
        right.ope_ == Ope::READ) {
      return true;
    } else if (this->key_ == right.key_ && this->ope_ == Ope::WRITE &&
               right.ope_ == Ope::WRITE) {
      return true;
    }
    /* キーが同値なら先に write ope を実行したい．read -> write よりも write ->
     * read.
     * キーが同値で自分が read でここまで来たら，下記の式によって絶対に false
     * となり，自分 (read) が昇順で後ろ回しになるので ok */

    return this->key_ < right.key_;
  }
};
