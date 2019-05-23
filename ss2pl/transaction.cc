
#include <stdio.h>
#include <string.h>

#include <atomic>

#include "../include/debug.hpp"

#include "include/common.hpp"
#include "include/transaction.hpp"

using namespace std;

inline
SetElement<Tuple> *
TxExecutor::searchReadSet(uint64_t key) 
{
  for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
    if ((*itr).key == key) return &(*itr);
  }

  return nullptr;
}

inline
SetElement<Tuple> *
TxExecutor::searchWriteSet(uint64_t key) 
{
  for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
    if ((*itr).key == key) return &(*itr);
  }

  return nullptr;
}

void
TxExecutor::abort()
{
  unlock_list();

  readSet.clear();
  writeSet.clear();
}

void
TxExecutor::commit()
{
  for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
    memcpy((*itr).rcdptr->val, writeVal, VAL_SIZE);
  }

  unlock_list();

  readSet.clear();
  writeSet.clear();
}

void
TxExecutor::tbegin()
{
  this->status = TransactionStatus::inFlight;
}

char*
TxExecutor::tread(uint64_t key)
{
  //if it already access the key object once.
  // w
  SetElement<Tuple> *inW = searchWriteSet(key);
  if (inW != nullptr) return writeVal;

  SetElement<Tuple> *inR = searchReadSet(key);
  if (inR != nullptr) return inR->val;

#if MASSTREE_USE
  Tuple *tuple = MT.get_value(key);
#else
  Tuple *tuple = get_tuple(Table, key);
#endif

  if (tuple->lock.r_trylock()) {
    r_lockList.emplace_back(&tuple->lock);
    readSet.emplace_back(key, tuple, tuple->val);
    
    return returnVal;
  } else {
    this->status = TransactionStatus::aborted;
    return nullptr;
  }
}

void
TxExecutor::twrite(uint64_t key)
{
  // if it already wrote the key object once.
  SetElement<Tuple> *inW = searchWriteSet(key);
  if (inW) return;

#if MASSTREE_USE
  Tuple *tuple = MT.get_value(key);
#else
  Tuple *tuple = get_tuple(Table, key);
#endif

  for (auto rItr = readSet.begin(); rItr != readSet.end(); ++rItr) {
    if ((*rItr).key == key) { // hit
      if (!tuple->lock.tryupgrade()) {
        this->status = TransactionStatus::aborted;
        return;
      }

      // upgrade success
      for (auto lItr = r_lockList.begin(); lItr != r_lockList.end(); ++lItr) {
        if (*lItr == &(tuple->lock)) {
          r_lockList.erase(lItr);
          w_lockList.emplace_back(&tuple->lock);
          writeSet.emplace_back(key);
          break;
        }
      }

      readSet.erase(rItr);
      return;
    }
  }

  // trylock
  if (!tuple->lock.w_trylock()) {
    this->status = TransactionStatus::aborted;
    return;
  }

  w_lockList.emplace_back(&tuple->lock);
  writeSet.emplace_back(key, tuple);
  return;
}

void
TxExecutor::unlock_list()
{
  for (auto itr = r_lockList.begin(); itr != r_lockList.end(); ++itr)
    (*itr)->r_unlock();

  for (auto itr = w_lockList.begin(); itr != w_lockList.end(); ++itr)
    (*itr)->w_unlock();

  r_lockList.clear();
  w_lockList.clear();
}

