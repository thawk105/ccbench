#pragma once

#include <cstdint>
#include <map>
#include <vector>

#include "../../include/util.hpp"

#include "garbageCollection.hpp"
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
  vector<SetElement> readSet;
  vector<SetElement> writeSet;
  GarbageCollection gcobject;
  uint32_t preGcThreshold = 0;
  Result rsobject;

  uint8_t thid; // thread ID
  uint32_t txid;  //TID and begin timestamp - the current log sequence number (LSN)

  char writeVal[VAL_SIZE] = {};

  TxExecutor(uint8_t newthid, unsigned int max_ope) : thid(newthid) {
    gcobject.thid = thid;
    readSet.reserve(max_ope);
    writeSet.reserve(max_ope);

    writeValGenerator(writeVal, VAL_SIZE, thid);
  }

  SetElement *searchReadSet(unsigned int key);
  SetElement *searchWriteSet(unsigned int key);
  void tbegin();
  char* tread(unsigned int key);
  void twrite(unsigned int key);
  void commit();
  void abort();
  void dispWS();
  void dispRS();
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

