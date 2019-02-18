#pragma once

#include "lock.hpp"
#include "tuple.hpp"
#include "common.hpp"
#include <vector>

using namespace std;

enum class TransactionStatus : uint8_t {
  inFlight,
  committed,
  aborted,
};

class TxExecutor {
public:
  vector<ReadElement> readSet;
  vector<WriteElement> writeSet;
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
  }

  ReadElement *searchReadSet(unsigned int key);
  WriteElement *searchWriteSet(unsigned int key);
  template <typename T> T *searchRLL(unsigned int key);
  void removeFromCLL(unsigned int key);
  void begin();
  unsigned int read(unsigned int key);
  void write(unsigned int key, unsigned int val);
  void lock(Tuple *tuple, bool mode);
  void construct_RLL(); // invoked on abort;
  void unlockCLL();
  bool commit();
  void abort();
  void writePhase();
  void dispCLL();
  void dispRLL();
  void dispWS();
};

