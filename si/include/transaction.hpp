#pragma once

#include <cstdint>
#include <map>
#include <vector>

#include "/home/tanabe/package/tbb/include/tbb/scalable_allocator.h"

#include "../../include/procedure.hpp"
#include "../../include/string.hpp"
#include "../../include/util.hpp"
#include "garbageCollection.hpp"
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

class TxExecutor {
public:
  uint32_t cstamp = 0;  // Transaction end time, c(T) 
  TransactionStatus status = TransactionStatus::inFlight;   // Status: inFlight, committed, or aborted
  std::vector<SetElement<Tuple>, tbb::scalable_allocator<SetElement<Tuple>>> readSet;
  std::vector<SetElement<Tuple>, tbb::scalable_allocator<SetElement<Tuple>>> writeSet;
  std::vector<Procedure> proSet;
  GarbageCollection gcobject;
  uint32_t preGcThreshold = 0;

  uint8_t thid_; // thread ID
  uint32_t txid;  //TID and begin timestamp - the current log sequence number (LSN)
  SIResult *sres_;

  char returnVal[VAL_SIZE] = {};
  char writeVal[VAL_SIZE] = {};

  TxExecutor(uint8_t thid, unsigned int max_ope, SIResult* sres) : thid_(thid), sres_(sres) {
    gcobject.thid = thid;
    readSet.reserve(max_ope);
    writeSet.reserve(max_ope);
    proSet.reserve(max_ope);

    genStringRepeatedNumber(writeVal, VAL_SIZE, thid);
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
  std::atomic<uint32_t> txid;
  std::atomic<uint32_t> lastcstamp;
  uint8_t padding[56];

  TransactionTable(uint32_t txid, uint32_t lastcstamp) {
    this->txid = txid;
    this->lastcstamp = lastcstamp;
  }
};

