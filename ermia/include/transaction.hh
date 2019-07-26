#pragma once

#include <cstdint>
#include <map>
#include <vector>

#include "../../include/string.hh"
#include "../../include/procedure.hh"
#include "../../include/util.hh"
#include "ermia_op_element.hh"
#include "garbage_collection.hh"
#include "result.hh"
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

using namespace std;

class TxExecutor {
public:
  uint32_t cstamp_ = 0;  // Transaction end time, c(T) 
  TransactionStatus status_ = TransactionStatus::inFlight;   // Status: inFlight, committed, or aborted
  uint32_t pstamp_ = 0;  // Predecessor high-water mark, η (T)
  uint32_t sstamp_ = UINT32_MAX; // Successor low-water mark, pi (T)
  vector<SetElement<Tuple>> read_set_;
  vector<SetElement<Tuple>> write_set_;
  GarbageCollection gcobject_;
  vector<Procedure> pro_set_;
  uint32_t pre_gc_threshold_ = 0;

  uint8_t thid_; // thread ID
  uint32_t txid_;  //TID and begin timestamp - the current log sequence number (LSN)
  ErmiaResult* eres_;

  uint64_t gcstart_, gcstop_; // counter for garbage collection

  char returnVal[VAL_SIZE] = {};
  char writeVal[VAL_SIZE] = {};

  TxExecutor(uint8_t thid, size_t max_ope, ErmiaResult* eres) : thid_(thid), eres_(eres) {
    gcobject_.set_thid_(thid);
    read_set_.reserve(max_ope);
    write_set_.reserve(max_ope);
    pro_set_.reserve(max_ope);

    genStringRepeatedNumber(writeVal, VAL_SIZE, thid);
  }

  SetElement<Tuple> *searchReadSet(unsigned int key);
  SetElement<Tuple> *searchWriteSet(unsigned int key);
  void tbegin();
  char* ssn_tread(unsigned int key);
  void ssn_twrite(unsigned int key);
  void ssn_commit();
  void ssn_parallel_commit();
  void abort();
  void mainte();
  void verify_exclusion_or_abort();
  void dispWS();
  void dispRS();

  void upReadersBits(Version *ver) {
    uint64_t expected, desired;
    for (;;) {
      expected = ver->readers_.load(memory_order_acquire);
RETRY_URB:
      if (expected & (1<<thid_)) break;
      desired = expected | (1<<thid_);
      if (ver->readers_.compare_exchange_weak(expected, desired, memory_order_acq_rel, memory_order_acquire)) break;
      else goto RETRY_URB;
    }
  }

  void downReadersBits(Version *ver) {
    uint64_t expected, desired;
    for (;;) {
      expected = ver->readers_.load(memory_order_acquire);
RETRY_DRB:
      if (!(expected & (1<<thid_))) break;
      desired = expected & ~(1<<thid_);
      if (ver->readers_.compare_exchange_weak(expected, desired, memory_order_acq_rel, memory_order_acquire)) break;
      else goto RETRY_DRB;
    }
  }

  static INLINE Tuple* get_tuple(Tuple *table, uint64_t key) {
    return &table[key];
  }
};

// for MVCC SSN
class TransactionTable {
public:
  alignas(CACHE_LINE_SIZE)
  std::atomic<uint32_t> txid_;
  std::atomic<uint32_t> cstamp_;
  std::atomic<uint32_t> sstamp_;
  std::atomic<uint32_t> lastcstamp_;
  std::atomic<TransactionStatus> status_;

  TransactionTable(uint32_t txid, uint32_t cstamp, uint32_t sstamp, uint32_t lastcstamp, TransactionStatus status) {
    this->txid_ = txid;
    this->cstamp_ = cstamp;
    this->sstamp_ = sstamp;
    this->lastcstamp_ = lastcstamp;
    this->status_ = status;
  }
};

