#pragma once

#include <pthread.h>
#include <string.h>

#include <atomic>
#include <cstdint>

struct Tidword {
  union {
    uint64_t obj;
    struct {
      bool lock:1;
      bool latest:1;
      bool absent:1;
      uint64_t tid:29;
      uint64_t epoch:32;
    };
  };

  Tidword() : obj(0) {};

  bool operator==(const Tidword& right) const {
    return obj == right.obj;
  }

  bool operator!=(const Tidword& right) const {
    return !operator==(right);
  }

  bool operator<(const Tidword& right) const {
    return this->obj < right.obj;
  }
};

class Tuple {
public:
  Tidword tidword;

  char keypad[KEY_SIZE];
  char val[VAL_SIZE];
};

class ReadElement {
public:
  Tidword tidword;
  unsigned int key;
  char val[VAL_SIZE];

  ReadElement(unsigned int newkey, char* newval, Tidword newtidword) : key(newkey) {
    tidword.obj = newtidword.obj;
    memcpy(this->val, newval, VAL_SIZE);
  }

  bool operator<(const ReadElement& right) const {
    return this->key < right.key;
  }
};

class WriteElement {
public:
  unsigned int key;

  WriteElement(unsigned int key) {
    this->key = key;
  }

  bool operator<(const WriteElement& right) const {
    return this->key < right.key;
  }
};
