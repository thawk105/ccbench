
#include <stdio.h>
#include <string.h>

#include <atomic>

#include "../include/debug.hh"
#include "../include/procedure.hh"
#include "../include/result.hh"
#include "include/common.hh"
#include "include/transaction.hh"

using namespace std;

extern void display_procedure_vector(std::vector<Procedure>& pro);

inline
SetElement<Tuple> *
TxExecutor::searchReadSet(uint64_t key) 
{
  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
    if ((*itr).key_ == key) return &(*itr);
  }

  return nullptr;
}

inline
SetElement<Tuple> *
TxExecutor::searchWriteSet(uint64_t key) 
{
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    if ((*itr).key_ == key) return &(*itr);
  }

  return nullptr;
}

void
TxExecutor::abort()
{
  unlockList();
  read_set_.clear();
  write_set_.clear();
  ++sres_->local_abort_counts_;
}

void
TxExecutor::commit()
{
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    memcpy((*itr).rcdptr_->val_, write_val_, VAL_SIZE);
  }

  unlockList();
  read_set_.clear();
  write_set_.clear();
  ++sres_->local_commit_counts_;
}

void
TxExecutor::tbegin()
{
  this->status_ = TransactionStatus::inFlight;
}

char*
TxExecutor::tread(uint64_t key)
{
  //if it already access the key object once.
  // w
  SetElement<Tuple> *inW = searchWriteSet(key);
  if (inW != nullptr) return write_val_;

  SetElement<Tuple> *inR = searchReadSet(key);
  if (inR != nullptr) return inR->val_;

#if MASSTREE_USE
  Tuple *tuple = MT.get_value(key);
#if ADD_ANALYSIS
  ++sres_->local_tree_traversal_;
#endif
#else
  Tuple *tuple = get_tuple(Table, key);
#endif

#ifdef DLR0
  tuple->lock_.r_lock();
  r_lock_list_.emplace_back(&tuple->lock_);
  read_set_.emplace_back(key, tuple, tuple->val_);
#elif defined ( DLR1 )
  if (tuple->lock_.r_trylock()) {
    r_lock_list_.emplace_back(&tuple->lock_);
    read_set_.emplace_back(key, tuple, tuple->val_);
  } else {
    this->status_ = TransactionStatus::aborted;
    return nullptr;
  }
#endif

  return return_val_;
}

void
TxExecutor::twrite(uint64_t key)
{
  // if it already wrote the key object once.
  SetElement<Tuple> *inW = searchWriteSet(key);
  if (inW) return;

  for (auto rItr = read_set_.begin(); rItr != read_set_.end(); ++rItr) {
    if ((*rItr).key_ == key) { // hit
#if DLR0
      (*rItr).rcdptr_->lock_.upgrade();
#elif defined ( DLR1 )
      if (!(*rItr).rcdptr_->lock_.tryupgrade()) {
        this->status_ = TransactionStatus::aborted;
        return;
      }
#endif

      // upgrade success
      for (auto lItr = r_lock_list_.begin(); lItr != r_lock_list_.end(); ++lItr) {
        if (*lItr == &((*rItr).rcdptr_->lock_)) {
          write_set_.emplace_back(key, (*rItr).rcdptr_);
          w_lock_list_.emplace_back(&(*rItr).rcdptr_->lock_);
          r_lock_list_.erase(lItr);
          break;
        }
      }

      read_set_.erase(rItr);
      return;
    }
  }

#if MASSTREE_USE
  Tuple *tuple = MT.get_value(key);
#if ADD_ANALYSIS
  ++sres_->local_tree_traversal_;
#endif
#else
  Tuple *tuple = get_tuple(Table, key);
#endif

#if DLR0
  tuple->lock_.w_lock();
#elif defined ( DLR1 )
  if (!tuple->lock_.w_trylock()) {
    this->status_ = TransactionStatus::aborted;
    return;
  }
#endif

  w_lock_list_.emplace_back(&tuple->lock_);
  write_set_.emplace_back(key, tuple);
  return;
}

void
TxExecutor::unlockList()
{
  for (auto itr = r_lock_list_.begin(); itr != r_lock_list_.end(); ++itr)
    (*itr)->r_unlock();

  for (auto itr = w_lock_list_.begin(); itr != w_lock_list_.end(); ++itr)
    (*itr)->w_unlock();

  r_lock_list_.clear();
  w_lock_list_.clear();
}

