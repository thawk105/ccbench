#pragma once

#include <vector>

#include "../../include/rwlock.hpp"
#include "../../include/string.hpp"
#include "../../include/util.hpp"

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
  std::vector<RWLock*> r_lockList;
  std::vector<RWLock*> w_lockList;
  TransactionStatus status = TransactionStatus::inFlight;

  vector<SetElement> readSet;
  vector<SetElement> writeSet;

  char writeVal[VAL_SIZE];
  char returnVal[VAL_SIZE];

  TxExecutor(int myid) : thid(myid) {
    readSet.reserve(MAX_OPE);
    writeSet.reserve(MAX_OPE);
    r_lockList.reserve(MAX_OPE);
    w_lockList.reserve(MAX_OPE);

    genStringRepeatedNumber(writeVal, VAL_SIZE, thid); 
  }

  SetElement *searchReadSet(unsigned int key);
  SetElement *searchWriteSet(unsigned int key);
  void tbegin();
  char* tread(unsigned int key);
  void twrite(unsigned int key);
  void commit();
  void abort();
  void unlock_list();
};
