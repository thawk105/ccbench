#pragma once

#include <iostream>
#include <set>
#include <vector>

#include "common.hpp"
#include "log.hpp"
#include "procedure.hpp"
#include "silo_op_element.hpp"
#include "tuple.hpp"

#include "../../include/fileio.hpp"
#include "../../include/result.hpp"
#include "../../include/string.hpp"

#define LOGSET_SIZE 1000

using namespace std;

class TxnExecutor {
public:
  vector<ReadElement<Tuple>> readSet;
  vector<WriteElement<Tuple>> writeSet;

  vector<LogRecord> logSet;
  LogHeader latestLogHeader;

  Result rsob;

  File logfile;

  unsigned int thid;
  Tidword mrctid;
  Tidword max_rset, max_wset;

  Result rsobject;

  char writeVal[VAL_SIZE];
  char returnVal[VAL_SIZE];

  TxnExecutor(int newthid) : thid(newthid) {
    readSet.reserve(MAX_OPE);
    writeSet.reserve(MAX_OPE);
    //logSet.reserve(LOGSET_SIZE);

    //latestLogHeader.init();

    max_rset.obj = 0;
    max_wset.obj = 0;

    genStringRepeatedNumber(writeVal, VAL_SIZE, thid);
  }

  void tbegin();
  char* tread(uint64_t key);
  void twrite(uint64_t key);
  bool validationPhase();
  void abort();
  void writePhase();
  void wal(uint64_t ctid);
  void lockWriteSet();
  void unlockWriteSet();
  ReadElement<Tuple> *searchReadSet(uint64_t key);
  WriteElement<Tuple> *searchWriteSet(uint64_t key);

  Tuple* get_tuple(Tuple *table, uint64_t key) {
    return &table[key];
  }
};
