#pragma once

#include <vector>

#include "../../include/procedure.hh"
#include "../../include/result.hh"
#include "../../include/string.hh"
#include "../../include/util.hh"
#include "common.hh"
#include "lock.hh"
#include "mocc_op_element.hh"
#include "tuple.hh"

using namespace std;

enum class TransactionStatus : uint8_t {
  inFlight,
  committed,
  aborted,
};

class TxExecutor {
public:
  vector <ReadElement<Tuple>> read_set_;
  vector <WriteElement<Tuple>> write_set_;
  vector <Procedure> pro_set_;
#ifdef RWLOCK
  vector<LockElement<RWLock>> RLL_;
  vector<LockElement<RWLock>> CLL_;
#endif  // RWLOCK
#ifdef MQLOCK
  vector<LockElement<MQLock>> RLL_;
  vector<LockElement<MQLock>> CLL_;
#endif  // MQLOCK
  TransactionStatus status_;

  int thid_;
  Tidword mrctid_;
  Tidword max_rset_;
  Tidword max_wset_;
  Xoroshiro128Plus *rnd_;
  Result *mres_;

  char write_val_[VAL_SIZE] = {};
  char return_val_[VAL_SIZE] = {};

  TxExecutor(int thid, Xoroshiro128Plus *rnd, Result *mres)
          : thid_(thid), mres_(mres) {
    read_set_.reserve(FLAGS_max_ope);
    write_set_.reserve(FLAGS_max_ope);
    pro_set_.reserve(FLAGS_max_ope);
    RLL_.reserve(FLAGS_max_ope);
    CLL_.reserve(FLAGS_max_ope);

    this->status_ = TransactionStatus::inFlight;
    this->rnd_ = rnd;
    max_rset_.obj_ = 0;
    max_wset_.obj_ = 0;

    genStringRepeatedNumber(write_val_, VAL_SIZE, thid);
  }

  ReadElement<Tuple> *searchReadSet(uint64_t key);

  WriteElement<Tuple> *searchWriteSet(uint64_t key);

  template<typename T>
  T *searchRLL(uint64_t key);

  void removeFromCLL(uint64_t key);

  void begin();

  void read(uint64_t key);

  void write(uint64_t key);

  void read_write(uint64_t key);

  void lock(uint64_t key, Tuple *tuple, bool mode);

  void construct_RLL();  // invoked on abort;
  void unlockCLL();

  bool commit();

  void abort();

  void writePhase();

  void dispCLL();

  void dispRLL();

  void dispWS();

  Tuple *get_tuple(Tuple *table, uint64_t key) { return &table[key]; }
};
