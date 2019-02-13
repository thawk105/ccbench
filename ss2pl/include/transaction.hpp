#pragma once

#include <vector>

#include "../../include/rwlock.hpp"

#include "tuple.hpp"

using namespace std;

enum class TransactionStatus : uint8_t {
  inFlight,
  committed,
  aborted,
};

class Transaction {
public:
  int thid;
  std::vector<RWLock*> r_lockList;
  std::vector<RWLock*> w_lockList;
  TransactionStatus status = TransactionStatus::inFlight;

  vector<SetElement> readSet;
  vector<SetElement> writeSet;

  Transaction(int myid) {
    this->thid = myid;
    readSet.reserve(MAX_OPE);
    writeSet.reserve(MAX_OPE);
    r_lockList.reserve(MAX_OPE);
    w_lockList.reserve(MAX_OPE);
  }

  SetElement *searchReadSet(unsigned int key);
  SetElement *searchWriteSet(unsigned int key);
  void tbegin();
  int tread(unsigned int key);
  void twrite(unsigned int key, unsigned int val);
  void commit();
  void abort();
  void unlock_list();
};
