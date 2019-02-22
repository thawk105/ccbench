#pragma once

#include <atomic>
#include <mutex>

#include "../../include/rwlock.hpp"

using namespace std;

class Tuple {
public:
  RWLock lock;
  char pad[4];

  char keypad[KEY_SIZE];
  char val[VAL_SIZE];
};

class SetElement {
public:
  unsigned int key;
  char *val;

  SetElement(unsigned int newkey, char *newval) : key(newkey), val(newval) {}
};
