#pragma once
#include <cstdint>

enum class Ope {
  TREAD, 
  TWRITE
};

class Procedure {
public:
  Ope ope = Ope::TREAD;
  int key = 0;
  int val = 0;
};
