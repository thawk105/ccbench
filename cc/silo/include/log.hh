#pragma once

#include <string.h>

#include <cstdint>
#include <memory>

class LogHeader {
public:
  int chkSum_ = 0;
  unsigned int logRecNum_ = 0;
  const std::size_t len_val_ = VAL_SIZE;

  void init() {
    chkSum_ = 0;
    logRecNum_ = 0;
  }

  void convertChkSumIntoComplementOnTwo() {
    chkSum_ ^= 0xffffffff;
    ++chkSum_;
  }
};

class LogRecord {
public:
  uint64_t tid_;
  std::string_view key_;
  char val_[VAL_SIZE];

  // computeChkSum() below sums every int-sized chunk of *this*, including
  // any trailing struct padding after val_, so both constructors zero
  // the entire object first to make those reads well-defined.
  LogRecord() {
    memset(this, 0, sizeof(LogRecord));
  }

  LogRecord(uint64_t tid, std::string_view key, char *val) {
    memset(this, 0, sizeof(LogRecord));
    tid_ = tid;
    key_ = key;
    memcpy(this->val_, val, VAL_SIZE);
  }

  int computeChkSum() {
    // compute checksum
    int chkSum = 0;
    int *itr = (int *) this;
    for (unsigned int i = 0; i < sizeof(LogRecord) / sizeof(int); ++i) {
      chkSum += (*itr);
      ++itr;
    }

    return chkSum;
  }
};

class LogPackage {
public:
  LogHeader header_;
  std::unique_ptr<LogRecord[]> log_records_;
};
