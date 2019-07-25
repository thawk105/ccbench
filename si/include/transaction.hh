#pragma once

#include <cstdint>
#include <map>
#include <vector>

#include "/home/tanabe/package/tbb/include/tbb/scalable_allocator.h"

#include "../../include/procedure.hh"
#include "../../include/string.hh"
#include "../../include/util.hh"
#include "garbage_collection.hh"
#include "tuple.hh"
#include "version.hh"

// forward declaration
class TransactionTable;

enum class TransactionStatus : uint8_t {
  inFlight,
  committing,
  committed,
  aborted,
};

class TxExecutor {
public:
  uint32_t cstamp_ = 0;  // Transaction end time, c(T) 
  TransactionStatus status_ = TransactionStatus::inFlight;   // Status: inFlight, committed, or aborted
  std::vector<SetElement<Tuple>, tbb::scalable_allocator<SetElement<Tuple>>> read_set_;
  std::vector<SetElement<Tuple>, tbb::scalable_allocator<SetElement<Tuple>>> write_set_;
  std::vector<Procedure> pro_set_;
  GarbageCollection gcobject_;
  uint32_t pre_gc_threshold_ = 0;

  uint8_t thid_; // thread ID
  uint32_t txid_;  //TID and begin timestamp - the current log sequence number (LSN)
  SIResult *sres_;

  char return_val_[VAL_SIZE] = {};
  char write_val_[VAL_SIZE] = {};

  TxExecutor(uint8_t thid, unsigned int max_ope, SIResult* sres) : thid_(thid), sres_(sres) {
    gcobject_.thid_ = thid;
    read_set_.reserve(max_ope);
    write_set_.reserve(max_ope);
    pro_set_.reserve(max_ope);

    genStringRepeatedNumber(write_val_, VAL_SIZE, thid);
  }

  SetElement<Tuple> *searchReadSet(uint64_t key);
  SetElement<Tuple> *searchWriteSet(uint64_t key);
  void tbegin();
  char* tread(uint64_t key);
  void twrite(uint64_t key);
  void commit();
  void abort();
  void dispWS();
  void dispRS();

  static Tuple* get_tuple(Tuple *table, uint64_t key) {
    return &table[key];
  }
};

// for MVCC SSN
class TransactionTable {
public:
  alignas(CACHE_LINE_SIZE)
  std::atomic<uint32_t> txid_;
  std::atomic<uint32_t> lastcstamp_;

  TransactionTable(uint32_t txid, uint32_t lastcstamp) {
    this->txid_ = txid;
    this->lastcstamp_ = lastcstamp;
  }
};

