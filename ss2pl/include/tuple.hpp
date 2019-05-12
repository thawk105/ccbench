#pragma once

#include <atomic>
#include <mutex>

#include "../../include/cache_line_size.hpp"
#include "../../include/inline.hpp"
#include "../../include/rwlock.hpp"

using namespace std;

class Tuple {
public:
  RWLock lock;

  char keypad[KEY_SIZE];
  char val[VAL_SIZE];

  int8_t pad[CACHE_LINE_SIZE - ((4 + KEY_SIZE + VAL_SIZE) % CACHE_LINE_SIZE)];
};

class SetElement {
public:
  unsigned int key;
  char *val;

  SetElement(unsigned int newkey, char *newval) : key(newkey), val(newval) {}
};
