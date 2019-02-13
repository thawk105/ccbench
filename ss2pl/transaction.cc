
#include <stdio.h>

#include <atomic>

#include "../include/debug.hpp"

#include "include/common.hpp"
#include "include/transaction.hpp"

using namespace std;

inline
SetElement *
Transaction::searchReadSet(unsigned int key) 
{
  for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
    if ((*itr).key == key) return &(*itr);
  }

  return nullptr;
}

inline
SetElement *
Transaction::searchWriteSet(unsigned int key) 
{
  for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
    if ((*itr).key == key) return &(*itr);
  }

  return nullptr;
}

void
Transaction::abort()
{
  unlock_list();

  readSet.clear();
  writeSet.clear();
}

void
Transaction::commit()
{
  for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
    Table[(*itr).key].val = (*itr).val;
  }

  unlock_list();

  readSet.clear();
  writeSet.clear();
}

void
Transaction::tbegin()
{
  this->status = TransactionStatus::inFlight;
}

int
Transaction::tread(unsigned int key)
{
  //if it already access the key object once.
  // w
  SetElement *inW = searchWriteSet(key);
  if (inW != nullptr) {
    return inW->val;
  }

  SetElement *inR = searchReadSet(key);
  if (inR != nullptr) {
    return inR->val;
  }

  if (Table[key].lock.r_trylock()) {
    r_lockList.emplace_back(&Table[key].lock);
    readSet.emplace_back(key, Table[key].val);
    return Table[key].val;
  } else {
    this->status = TransactionStatus::aborted;
    return -1;
  }
}

void
Transaction::twrite(unsigned int key, unsigned int val)
{
  // if it already wrote the key object once.
  SetElement *inW = searchWriteSet(key);
  if (inW) {
    inW->val = val;
    return;
  }

  for (auto rItr = readSet.begin(); rItr != readSet.end(); ++rItr) {
    if ((*rItr).key == key) { // hit
      if (!Table[key].lock.tryupgrade()) {
        this->status = TransactionStatus::aborted;
        return;
      }

      // upgrade success
      for (auto lItr = r_lockList.begin(); lItr != r_lockList.end(); ++lItr) {
        if (*lItr == &(Table[key].lock)) {
          r_lockList.erase(lItr);
          w_lockList.emplace_back(&Table[key].lock);
          writeSet.emplace_back(key, val);
          break;
        }
      }

      readSet.erase(rItr);
      return;
    }
  }

  // trylock
  if (!Table[key % TUPLE_NUM].lock.w_trylock()) {
    this->status = TransactionStatus::aborted;
    return;
  }

  w_lockList.emplace_back(&Table[key % TUPLE_NUM].lock);
  writeSet.emplace_back(key, val);
  return;
}

void
Transaction::unlock_list()
{
  for (auto itr = r_lockList.begin(); itr != r_lockList.end(); ++itr)
    (*itr)->r_unlock();

  for (auto itr = w_lockList.begin(); itr != w_lockList.end(); ++itr)
    (*itr)->w_unlock();

  r_lockList.clear();
  w_lockList.clear();
}

