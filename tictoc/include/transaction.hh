#pragma once

#include <string.h>

#include <iostream>
#include <set>
#include <vector>

#include "../../include/inline.hh"
#include "../../include/procedure.hh"
#include "../../include/result.hh"
#include "../../include/string.hh"
#include "../../include/util.hh"
#include "common.hh"
#include "tictoc_op_element.hh"
#include "tuple.hh"

enum class TransactionStatus : uint8_t {
  inFlight,
  committed,
  aborted,
};

using namespace std;

extern void write_val_Generator(char* write_val_, size_t val_size, size_t thid);

class TxExecutor {
 public:
  int thid_;
  uint64_t commit_ts_;
  uint64_t appro_commit_ts_;
  Result* tres_;
  bool wonly_ = false;
  vector<Procedure> pro_set_;

  TransactionStatus status_;
  vector<SetElement<Tuple>> read_set_;
  vector<SetElement<Tuple>> write_set_;
  vector<OpElement<Tuple>> cll_;  // current lock list;
  // use for lockWriteSet() to record locks;

  char write_val_[VAL_SIZE];
  char return_val_[VAL_SIZE];

  TxExecutor(int thid, Result* tres) : thid_(thid), tres_(tres) {
    read_set_.reserve(MAX_OPE);
    write_set_.reserve(MAX_OPE);
    cll_.reserve(MAX_OPE);
    pro_set_.reserve(MAX_OPE);

    genStringRepeatedNumber(write_val_, VAL_SIZE, thid);
  }

  void begin();
  void read(uint64_t key);
  void write(uint64_t key);
  bool validationPhase();
  bool preemptiveAborts(const TsWord& v1);
  void abort();
  void writePhase();
  void lockWriteSet();
  void unlockCLL();
  SetElement<Tuple>* searchWriteSet(uint64_t key);
  SetElement<Tuple>* searchReadSet(uint64_t key);
  void dispWS();

  Tuple* get_tuple(Tuple* table, uint64_t key) { return &table[key]; }
};
