#pragma once

#include <atomic>
#include <cstdint>
#include <pthread.h>

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
  unsigned int key = 0;
  unsigned int val=0;
};

class ReadElement {
public:
  Tidword tidword;
  unsigned int key;
  unsigned int val;

  ReadElement(unsigned int newkey, unsigned int newval, Tidword newtidword) {
   tidword.obj = newtidword.obj;
   key = newkey;
   val = newval;
  }

  bool operator<(const ReadElement& right) const {
    return this->key < right.key;
  }
};

class WriteElement {
public:
  unsigned int key, val;

  WriteElement(unsigned int key, unsigned int val) {
    this->key = key;
    this->val = val;
  }

  bool operator<(const WriteElement& right) const {
    return this->key < right.key;
  }
};
