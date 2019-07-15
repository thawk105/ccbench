#pragma once

#include <vector>

#include "../../include/string.hpp"
#include "../../include/procedure.hpp"
#include "../../include/util.hpp"

#include "common.hpp"
#include "mocc_op_element.hpp"
#include "lock.hpp"
#include "result.hpp"
#include "tuple.hpp"

using namespace std;

enum class TransactionStatus : uint8_t {
  inFlight,
  committed,
  aborted,
};

class TxExecutor {
public:
  vector<ReadElement<Tuple>> readSet;
  vector<WriteElement<Tuple>> writeSet;
  vector<Procedure> proSet;
#ifdef RWLOCK
  vector<LockElement<RWLock>> RLL;
  vector<LockElement<RWLock>> CLL;
#endif // RWLOCK
#ifdef MQLOCK
  vector<LockElement<MQLock>> RLL;
  vector<LockElement<MQLock>> CLL;
#endif // MQLOCK
  TransactionStatus status;

  int thid_;
  Tidword mrctid;
  Tidword max_rset;
  Tidword max_wset;
  Xoroshiro128Plus *rnd;
  MoccResult* tres_;
  
  char writeVal[VAL_SIZE] = {};
  char returnVal[VAL_SIZE] = {};

  TxExecutor(int thid, Xoroshiro128Plus *rnd, MoccResult* tres) : thid_(thid), tres_(tres) {
    readSet.reserve(MAX_OPE);
    writeSet.reserve(MAX_OPE);
    proSet.reserve(MAX_OPE);
    RLL.reserve(MAX_OPE);
    CLL.reserve(MAX_OPE);

    this->status = TransactionStatus::inFlight;
    this->rnd = rnd;
    max_rset.obj = 0;
    max_wset.obj = 0;

    genStringRepeatedNumber(writeVal, VAL_SIZE, thid);
  }

  ReadElement<Tuple> *searchReadSet(uint64_t key);
  WriteElement<Tuple> *searchWriteSet(uint64_t key);
  template <typename T> T *searchRLL(uint64_t key);
  void removeFromCLL(uint64_t key);
  void begin();
  char* read(uint64_t key);
  void write(uint64_t key);
  void lock(uint64_t key, Tuple *tuple, bool mode);
  void construct_RLL(); // invoked on abort;
  void unlockCLL();
  bool commit();
  void abort();
  void writePhase();
  void dispCLL();
  void dispRLL();
  void dispWS();

  Tuple* get_tuple(Tuple *table, uint64_t key) {
    return &table[key];
  }
};

