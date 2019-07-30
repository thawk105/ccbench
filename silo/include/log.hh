#pragma once

#include <string.h>

#include <cstdint>

#define LOGLIST_SIZE 31

class LogHeader {
 public:
  int chkSum = 0;
  unsigned int logRecNum = 0;
  // 8 bytes

  void init() {
    chkSum = 0;
    logRecNum = 0;
  }

  void convertChkSumIntoComplementOnTwo() {
    chkSum ^= 0xffffffff;
    ++chkSum;
  }
};

class LogRecord {
 public:
  uint64_t tid;
  unsigned int key;
  char val[VAL_SIZE];
  // 16 bytes
  //
  LogRecord() : tid(0), key(0) {}

  LogRecord(uint64_t tid, unsigned int key, char *val) {
    this->tid = tid;
    this->key = key;
    memcpy(this->val, val, VAL_SIZE);
  }

  int computeChkSum() {
    // compute checksum
    int chkSum = 0;
    int *itr = (int *)this;
    for (unsigned int i = 0; i < sizeof(LogRecord) / sizeof(int); ++i) {
      chkSum += (*itr);
      ++itr;
    }

    return chkSum;
  }
};
