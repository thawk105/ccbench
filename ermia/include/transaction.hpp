#pragma once

#include <cstdint>
#include <map>
#include <vector>

#include "../../include/string.hpp"
#include "../../include/procedure.hpp"
#include "../../include/util.hpp"
#include "ermia_op_element.hpp"
#include "garbageCollection.hpp"
#include "result.hpp"
#include "tuple.hpp"
#include "version.hpp"

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
  uint32_t cstamp = 0;  // Transaction end time, c(T) 
  TransactionStatus status = TransactionStatus::inFlight;   // Status: inFlight, committed, or aborted
  uint32_t pstamp = 0;  // Predecessor high-water mark, η (T)
  uint32_t sstamp = UINT32_MAX; // Successor low-water mark, pi (T)
  vector<SetElement<Tuple>> readSet;
  vector<SetElement<Tuple>> writeSet;
  GarbageCollection gcobject;
  vector<Procedure> proSet;
  uint32_t preGcThreshold = 0;

  uint8_t thid_; // thread ID
  uint32_t txid;  //TID and begin timestamp - the current log sequence number (LSN)
  ErmiaResult* eres_;

  uint64_t gcstart, gcstop; // counter for garbage collection

  char returnVal[VAL_SIZE] = {};
  char writeVal[VAL_SIZE] = {};

  TxExecutor(uint8_t thid, size_t max_ope, ErmiaResult* eres) : thid_(thid), eres_(eres) {
    gcobject.set_thid_(thid);
    readSet.reserve(max_ope);
    writeSet.reserve(max_ope);
    proSet.reserve(max_ope);

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
      expected = ver->readers.load(memory_order_acquire);
RETRY_URB:
      if (expected & (1<<thid_)) break;
      desired = expected | (1<<thid_);
      if (ver->readers.compare_exchange_weak(expected, desired, memory_order_acq_rel, memory_order_acquire)) break;
      else goto RETRY_URB;
    }
  }

  void downReadersBits(Version *ver) {
    uint64_t expected, desired;
    for (;;) {
      expected = ver->readers.load(memory_order_acquire);
RETRY_DRB:
      if (!(expected & (1<<thid_))) break;
      desired = expected & ~(1<<thid_);
      if (ver->readers.compare_exchange_weak(expected, desired, memory_order_acq_rel, memory_order_acquire)) break;
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
  std::atomic<uint32_t> txid;
  std::atomic<uint32_t> cstamp;
  std::atomic<uint32_t> sstamp;
  std::atomic<uint32_t> lastcstamp;
  std::atomic<TransactionStatus> status;
  uint8_t padding[15];

  TransactionTable(uint32_t txid, uint32_t cstamp, uint32_t sstamp, uint32_t lastcstamp, TransactionStatus status) {
    this->txid = txid;
    this->cstamp = cstamp;
    this->sstamp = sstamp;
    this->lastcstamp = lastcstamp;
    this->status = status;
  }
};

