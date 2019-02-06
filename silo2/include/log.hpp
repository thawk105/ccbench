#pragma once

#include <cstdint>

#define LOGLIST_SIZE 31

class LogRecord {
public:
  uint64_t tid;
  unsigned int key;
  unsigned int val; // 16bytes
};

class LogPack {
public:
  int chkSum;
  unsigned int index = 0; // index of filled logList.
  uint8_t padding[8] = {};
  LogRecord logList[LOGLIST_SIZE];

  void init() { chkSum = 0; index = 0; }

  bool put(uint64_t tid, unsigned int key, unsigned val) {
    if (index == LOGLIST_SIZE) return false;

    logList[index].tid = tid; 
    logList[index].key = key;
    logList[index].val = val;
    ++index;
    return true;
  }

  void computeChkSum() {
    // compute checksum
    int tmpChkSum = 0;
    int *itr = (int *)this;
    for (int i = 0; i < LOGLIST_SIZE; ++i) {
      tmpChkSum += (*itr);
      ++itr;
    }

    tmpChkSum ^= 0xffffffff;
    chkSum = ++tmpChkSum;
  }

};
