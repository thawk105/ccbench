#pragma once

enum class Ope {
  READ,
  WRITE,
};

class Procedure {
public:
  Ope ope = Ope::READ;
  uint64_t key = 0;
  uint64_t val = 0;

  bool operator<(const Procedure& right) const {
    return this->key < right.key;
  }
};
