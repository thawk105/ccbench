#pragma once

#include <vector>

#include "../../include/string.hpp"
#include "../../include/util.hpp"

#include "common.hpp"
#include "lock.hpp"
#include "result.hpp"
#include "tuple.hpp"

using namespace std;

enum class TransactionStatus : uint8_t {
  inFlight,
  committed,
  aborted,
};

class TxExecutor {
public:
  vector<ReadElement> readSet;
  vector<unsigned int> writeSet;
#ifdef RWLOCK
  vector<LockElement<RWLock>> RLL;
  vector<LockElement<RWLock>> CLL;
#endif // RWLOCK
#ifdef MQLOCK
  vector<LockElement<MQLock>> RLL;
  vector<LockElement<MQLock>> CLL;
#endif // MQLOCK
  TransactionStatus status;

  int thid;
  Tidword mrctid;
  Tidword max_rset;
  Tidword max_wset;
  Xoroshiro128Plus *rnd;
  int locknum; // corresponding to index of MQLNodeList.

  char writeVal[VAL_SIZE] = {};
  char returnVal[VAL_SIZE] = {};

  TxExecutor(int thid, Xoroshiro128Plus *rnd, int locknum) {
    readSet.reserve(MAX_OPE);
    writeSet.reserve(MAX_OPE);
    RLL.reserve(MAX_OPE);
    CLL.reserve(MAX_OPE);

    this->thid = thid;
    this->status = TransactionStatus::inFlight;
    this->rnd = rnd;
    max_rset.obj = 0;
    max_wset.obj = 0;
    this->locknum = locknum;

    genStringRepeatedNumber(writeVal, VAL_SIZE, thid);
  }

  ReadElement *searchReadSet(unsigned int key);
  unsigned int *searchWriteSet(unsigned int key);
  template <typename T> T *searchRLL(unsigned int key);
  void removeFromCLL(unsigned int key);
  void begin();
  char* read(unsigned int key);
  void write(unsigned int key);
  void lock(unsigned int key, Tuple *tuple, bool mode);
  void construct_RLL(); // invoked on abort;
  void unlockCLL();
  bool commit(MoccResult &res);
  void abort();
  void writePhase();
  void dispCLL();
  void dispRLL();
  void dispWS();
};

