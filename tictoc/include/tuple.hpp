#pragma once

#include <pthread.h>
#include <string.h>

#include <atomic>
#include <cstdint>

#include "../../include/cache_line_size.hpp"

struct TsWord {
  union {
    uint64_t obj;
    struct {
      bool lock:1;
      uint16_t delta:15;
      uint64_t wts:48;
    };
  };

  TsWord () {
    obj = 0;
  }

  bool operator==(const TsWord& right) const {
    return obj == right.obj;
  }

  bool operator!=(const TsWord& right) const {
    return !operator==(right);
  }

  bool isLocked() {
    if (lock) return true;
    else return false;
  }

  uint64_t rts() {
    return wts + delta;
  }

};
class Tuple {
public:
  TsWord tsw;
  TsWord pre_tsw;
  // wts 48bit, rts-wts 15bit, lockbit 1bit
  //

  char keypad[KEY_SIZE];
  char val[VAL_SIZE];

#ifdef SHAVE_REC
  int8_t pad[8];
#endif // SHAVE_REC
#ifndef SHAVE_REC
  int8_t pad[CACHE_LINE_SIZE - ((16 + KEY_SIZE + VAL_SIZE) % CACHE_LINE_SIZE)];
#endif // SHAVE_REC
};

class SetElement {
public:
  unsigned int key;
  char val[VAL_SIZE];
  TsWord tsw;

  SetElement(unsigned int newkey, char *newval, TsWord tsw) : key(newkey) {
    memcpy(val, newval, VAL_SIZE);
    this->tsw.obj = tsw.obj;
  }

  SetElement(unsigned int newkey, TsWord tsw) : key(newkey) {
    this->tsw.obj = tsw.obj;
  }

  bool operator<(const SetElement& right) const {
    return this->key < right.key;
  }
};

class LockElement {
public:
  unsigned int key;

  LockElement(unsigned int newkey) : key(newkey) {}
};
