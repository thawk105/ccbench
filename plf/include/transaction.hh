#pragma once

#include <vector>
#include <atomic>
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

  void warmupTuple(uint64_t key);

  void begin();

  void read(uint64_t key);

  void write(uint64_t key);

  void readWrite(uint64_t key);

  void commit();

  void validationPhase();

  //void warmupTuple(uint64_t key);

  void abort();

  void unlockList(bool is_commit);

  // inline
  Tuple *get_tuple(Tuple *table, uint64_t key) { return &table[key]; }

  bool conflict(LockType x, LockType y);

  void checkWound(vector<int> &list, LockType lock_type, Tuple *tuple);

  void PromoteWaiters(Tuple *tuple);

  void writelockAcquire(LockType lock_type, uint64_t key, Tuple *tuple);

  bool LockRelease(uint64_t key, Tuple *tuple);

  bool spinWait(uint64_t key, Tuple *tuple);

  bool lockUpgrade(uint64_t key, Tuple *tuple);

  void checkLists(uint64_t key, Tuple *tuple);

  void eraseFromLists(Tuple *tuple); // erase txn from waiters and owners lists in case of abort during spinwait

  vector<int>::iterator woundRelease(int txn, Tuple *tuple);

  bool readlockAcquire(LockType lock_type, uint64_t key, Tuple *tuple);

  bool readWait(Tuple *tuple, uint64_t key);

  void mtx_get();

  void mtx_release();
 
};
