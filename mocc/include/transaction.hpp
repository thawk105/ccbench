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
  vector<ReadElement<Tuple>> readSet_;
  vector<WriteElement<Tuple>> writeSet_;
  vector<Procedure> proSet_;
#ifdef RWLOCK
  vector<LockElement<RWLock>> RLL_;
  vector<LockElement<RWLock>> CLL_;
#endif // RWLOCK
#ifdef MQLOCK
  vector<LockElement<MQLock>> RLL_;
  vector<LockElement<MQLock>> CLL_;
#endif // MQLOCK
  TransactionStatus status_;

  int thid_;
  Tidword mrctid_;
  Tidword max_rset_;
  Tidword max_wset_;
  Xoroshiro128Plus *rnd_;
  MoccResult* mres_;
  
  char write_val_[VAL_SIZE] = {};
  char return_val_[VAL_SIZE] = {};

  TxExecutor(int thid, Xoroshiro128Plus *rnd, MoccResult* mres) : thid_(thid), mres_(mres) {
    readSet_.reserve(MAX_OPE);
    writeSet_.reserve(MAX_OPE);
    proSet_.reserve(MAX_OPE);
    RLL_.reserve(MAX_OPE);
    CLL_.reserve(MAX_OPE);

    this->status_ = TransactionStatus::inFlight;
    this->rnd_ = rnd;
    max_rset_.obj_ = 0;
    max_wset_.obj_ = 0;

    genStringRepeatedNumber(write_val_, VAL_SIZE, thid);
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

