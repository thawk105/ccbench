#pragma once

#include <vector>

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
  std::vector<RWLock *> r_lock_list_;
  std::vector<RWLock *> w_lock_list_;
  TransactionStatus status_ = TransactionStatus::inFlight;
  Result *sres_;
  vector <SetElement<Tuple>> read_set_;
  vector <SetElement<Tuple>> write_set_;
  vector <Procedure> pro_set_;

  char write_val_[VAL_SIZE];
  char return_val_[VAL_SIZE];

  TxExecutor(int thid, Result *sres) : thid_(thid), sres_(sres) {
    read_set_.reserve(FLAGS_max_ope);
    write_set_.reserve(FLAGS_max_ope);
    pro_set_.reserve(FLAGS_max_ope);
    r_lock_list_.reserve(FLAGS_max_ope);
    w_lock_list_.reserve(FLAGS_max_ope);

    genStringRepeatedNumber(write_val_, VAL_SIZE, thid);
  }

  SetElement<Tuple> *searchReadSet(uint64_t key);

  SetElement<Tuple> *searchWriteSet(uint64_t key);

  void begin();

  void read(uint64_t key);

  void write(uint64_t key);

  void readWrite(uint64_t key);

  void commit();

  void abort();

  void unlockList();

  // inline
  Tuple *get_tuple(Tuple *table, uint64_t key) { return &table[key]; }
};
