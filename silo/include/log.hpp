#pragma once

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
};

class LogRecord {
public:
  uint64_t tid;
  unsigned int key;
  unsigned int val; 
  // 16 bytes

  LogRecord(uint64_t tid, unsigned int key, unsigned int val) {
    this->tid = tid;
    this->key = key;
    this->val = val;
  }

  int computeChkSum() {
    // compute checksum
    int chkSum = 0;
    int *itr = (int *)this;
    for (unsigned int i = 0; i < sizeof(LogRecord) / sizeof(int); ++i) {
      chkSum += (*itr);
      ++itr;
    }

    chkSum ^= 0xffffffff;
    return ++chkSum;
  }
};
