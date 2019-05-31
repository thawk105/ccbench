#pragma once

enum class Ope : uint8_t {
  READ,
  WRITE,
};

class Procedure {
public:
  Ope ope = Ope::READ;
  uint64_t key;

  bool operator<(const Procedure& right) const {
    /*if (this->key == right.key) {
      if (this->ope == Ope::WRITE) return true;
    }*/
    return this->key < right.key;
  }
};
