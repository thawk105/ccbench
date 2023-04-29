#pragma once

#include <atomic>
#include <mutex>
#include <bitset>

#include "../../include/cache_line_size.hh"
#include "../../include/inline.hh"
#include "../../include/rwlock.hh"

using namespace std;

enum LockType 
{
    SH = -1,
    EX = 1
};

class Tuple
{
public:
  alignas(CACHE_LINE_SIZE) RWLock lock_;
  vector<int> owners;  
  vector<int> waiters; 
  char val_[VAL_SIZE];
  int8_t req_type[224] = {0}; // read -1 : write 1 : no touch 0

  Tuple() {
    owners.reserve(224);
    waiters.reserve(224);
  }

  bool sortAdd(int txn, vector<int> &list);
  void ownersAdd(int txn);
  bool ownersRemove(int txn);
  bool ownersCheck(int txn);
  bool remove(int txn, vector<int> &list);
  vector<int>::iterator itrRemove(int txn);
};
