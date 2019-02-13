#pragma once

#include <atomic>
#include <mutex>

#include "../../include/rwlock.hpp"

using namespace std;

class Tuple {
public:
  RWLock lock;
  atomic<unsigned int> key;
  atomic<unsigned int> val;

  uint8_t padding[20];
};

class SetElement {
public:
  unsigned int key;
  unsigned int val;

  SetElement(unsigned int key, unsigned int val) {
    this->key = key;
    this->val = val;
  }
};
