#pragma once

#include <iostream>

using std::cout;
using std::endl;

enum class Ope : uint8_t {
  READ,
  WRITE,
};

class Procedure {
public:
  Ope ope;
  uint64_t key;

  Procedure() : ope(Ope::READ), key(0){}
  Procedure(Ope ope_, uint64_t key_) : ope(ope_), key(key_) {}

  bool operator<(const Procedure& right) const {
    if (this->key == right.key && this->ope == Ope::WRITE && right.ope == Ope::READ) {
      return true;
    }
    else if (this->key == right.key && this->ope == Ope::WRITE && right.ope == Ope::WRITE) {
      return true;
    }
    /* キーが同値なら先に write ope を実行したい．read -> write よりも write -> read.*/

    return this->key < right.key;
  }
};

