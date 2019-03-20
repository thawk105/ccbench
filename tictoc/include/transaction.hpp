#pragma once

#include <string.h>

#include <iostream>
#include <set>
#include <vector>

#include "../../include/string.hpp"
#include "../../include/util.hpp"
#include "../../include/inline.hpp"

#include "common.hpp"
#include "procedure.hpp"
#include "tuple.hpp"

enum class TransactionStatus : uint8_t {
  inFlight,
  committed,
  aborted,
};

extern void writeValGenerator(char *writeVal, size_t val_size, size_t thid);

class TxExecutor {
public:
  int thid;
  uint64_t commit_ts;
  uint64_t appro_commit_ts;

  TransactionStatus status;
  vector<SetElement> readSet;
  vector<SetElement> writeSet;
  vector<unsigned int> cll; // current lock list;
  //use for lockWriteSet() to record locks;

  char writeVal[VAL_SIZE];
  char returnVal[VAL_SIZE];

  TxExecutor(int myid) : thid(myid) {
    readSet.reserve(MAX_OPE);
    writeSet.reserve(MAX_OPE);
    cll.reserve(MAX_OPE);

    genStringRepeatedNumber(writeVal, VAL_SIZE, thid);
  }

  void tbegin();
  char* tread(unsigned int key);
  void twrite(unsigned int key);
  bool validationPhase();
  void abort();
  void writePhase();
  void lockWriteSet();
  void unlockCLL();
  SetElement *searchWriteSet(unsigned int key);
  SetElement *searchReadSet(unsigned int key);
  void dispWS();
};
