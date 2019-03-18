#pragma once

#include <cstdint>
#include <map>
#include <vector>

#include "../../include/util.hpp"

#include "garbageCollection.hpp"
#include "result.hpp"
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

extern void writeValGenerator(char *writeVal, size_t val_size, size_t thid);

class TxExecutor {
public:
  uint32_t cstamp = 0;  // Transaction end time, c(T) 
  TransactionStatus status = TransactionStatus::inFlight;   // Status: inFlight, committed, or aborted
  uint32_t pstamp = 0;  // Predecessor high-water mark, η (T)
  uint32_t sstamp = UINT32_MAX; // Successor low-water mark, pi (T)
  vector<SetElement> readSet;
  vector<SetElement> writeSet;
  GarbageCollection gcobject;
  uint32_t preGcThreshold = 0;

  uint8_t thid; // thread ID
  uint32_t txid;  //TID and begin timestamp - the current log sequence number (LSN)

  Result rsobject;
  uint64_t gcstart, gcstop; // counter for garbage collection

  char returnVal[VAL_SIZE] = {};
  char writeVal[VAL_SIZE] = {};

  TxExecutor(uint8_t newThid, unsigned int max_ope) {
    this->thid = newThid;
    gcobject.thid = thid;
    readSet.reserve(max_ope);
    writeSet.reserve(max_ope);

    writeValGenerator(writeVal, VAL_SIZE, thid);
  }

  SetElement *searchReadSet(unsigned int key);
  SetElement *searchWriteSet(unsigned int key);
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
      if (expected & (1<<thid)) break;
      desired = expected | (1<<thid);
      if (ver->readers.compare_exchange_weak(expected, desired, memory_order_acq_rel, memory_order_acquire)) break;
      else goto RETRY_URB;
    }
  }

  void downReadersBits(Version *ver) {
    uint64_t expected, desired;
    for (;;) {
      expected = ver->readers.load(memory_order_acquire);
RETRY_DRB:
      if (!(expected & (1<<thid))) break;
      desired = expected & ~(1<<thid);
      if (ver->readers.compare_exchange_weak(expected, desired, memory_order_acq_rel, memory_order_acquire)) break;
      else goto RETRY_DRB;
    }
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

