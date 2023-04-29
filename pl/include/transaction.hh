#pragma once

#include <vector>
#include <mutex>

#include "../../include/procedure.hh"
#include "../../include/result.hh"
#include "../../include/rwlock.hh"
#include "../../include/string.hh"
#include "../../include/util.hh"
#include "ss2pl_op_element.hh"
#include "tuple.hh"

enum class TransactionStatus : uint8_t {
  inFlight,
  committed,
  aborted,
};

extern void writeValGenerator(char *writeVal, size_t val_size, size_t thid);

class TxExecutor {
public:
  alignas(CACHE_LINE_SIZE) int thid_;
  int txid_;
  TransactionStatus status_ = TransactionStatus::inFlight;
  Result *sres_;
  vector <SetElement<Tuple>> read_set_;
  vector <SetElement<Tuple>> write_set_;
  bool committing = false;
  vector <Procedure> pro_set_;
  char write_val_[VAL_SIZE];
  char return_val_[VAL_SIZE];

  std::mutex mtx;
  
  TxExecutor(int thid, Result *sres) : thid_(thid), sres_(sres), txid_(thid) {
    read_set_.reserve(FLAGS_max_ope);
    write_set_.reserve(FLAGS_max_ope);
    pro_set_.reserve(FLAGS_max_ope);

    genStringRepeatedNumber(write_val_, VAL_SIZE, thid);
    genStringRepeatedNumber(return_val_, VAL_SIZE, thid);
  }

  SetElement<Tuple> *searchReadSet(uint64_t key);

  SetElement<Tuple> *searchWriteSet(uint64_t key);

  void begin();

  void read(uint64_t key);

  void write(uint64_t key);

  void readWrite(uint64_t key);

  void commit();

  void detectionPhase();

  void abort();

  void unlockList(bool is_commit);

  // inline
  Tuple *get_tuple(Tuple *table, uint64_t key) { return &table[key]; }

  bool conflict(LockType x, LockType y);

  void letsWound(vector<int> &list, LockType lock_type, Tuple *tuple);

  void LockWr(LockType lock_type, Tuple *tuple);

  bool Unlock( uint64_t key, Tuple *tuple);

  bool letsWait(uint64_t key, Tuple *tuple);

  bool LockUpgr(uint64_t key, Tuple *tuple);

  vector<int>::iterator byeWound(int txn, Tuple *tuple);

  bool LockRd(LockType lock_type, Tuple *tuple);

  bool checkRd(int txn, Tuple *tuple);

  void mtx_get();

  void mtx_release();
};
