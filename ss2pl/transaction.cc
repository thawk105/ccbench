
#include <stdio.h>
#include <string.h>

#include <atomic>

#include "../include/debug.hh"
#include "../include/procedure.hh"
#include "../include/result.hh"
#include "include/common.hh"
#include "include/transaction.hh"

using namespace std;

extern void display_procedure_vector(std::vector<Procedure> &pro);

inline SetElement<Tuple> *TxExecutor::searchReadSet(uint64_t key) {
  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
    if ((*itr).key_ == key) return &(*itr);
  }

  return nullptr;
}

inline SetElement<Tuple> *TxExecutor::searchWriteSet(uint64_t key) {
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    if ((*itr).key_ == key) return &(*itr);
  }

  return nullptr;
}

void TxExecutor::abort() {
  unlockList();
  read_set_.clear();
  write_set_.clear();
  ++sres_->local_abort_counts_;
}

void TxExecutor::commit() {
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    memcpy((*itr).rcdptr_->val_, write_val_, VAL_SIZE);
  }

  unlockList();
  read_set_.clear();
  write_set_.clear();
  ++sres_->local_commit_counts_;
}

void TxExecutor::tbegin() { this->status_ = TransactionStatus::inFlight; }

void TxExecutor::tread(uint64_t key) {
#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif // ADD_ANALYSIS

  // if it already access the key object once.
  if (searchWriteSet(key) || searchReadSet(key)) goto FINISH_TREAD;

  Tuple *tuple;
#if MASSTREE_USE
  tuple = MT.get_value(key);
#if ADD_ANALYSIS
  ++sres_->local_tree_traversal_;
#endif
#else
  tuple = get_tuple(Table, key);
#endif

#ifdef DLR0
  tuple->lock_.r_lock();
  r_lock_list_.emplace_back(&tuple->lock_);
  read_set_.emplace_back(key, tuple, tuple->val_);
#elif defined(DLR1)
  if (tuple->lock_.r_trylock()) {
    r_lock_list_.emplace_back(&tuple->lock_);
    read_set_.emplace_back(key, tuple, tuple->val_);
  } else {
    this->status_ = TransactionStatus::aborted;
    goto FINISH_TREAD;
  }
#endif

FINISH_TREAD:
#if ADD_ANALYSIS
  sres_->local_read_latency_ += rdtscp() - start;
#endif
  return;
}

void TxExecutor::twrite(uint64_t key) {
#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif

  // if it already wrote the key object once.
  if (searchWriteSet(key)) goto FINISH_TWRITE;

  for (auto rItr = read_set_.begin(); rItr != read_set_.end(); ++rItr) {
    if ((*rItr).key_ == key) {  // hit
#if DLR0
      (*rItr).rcdptr_->lock_.upgrade();
#elif defined(DLR1)
      if (!(*rItr).rcdptr_->lock_.tryupgrade()) {
        this->status_ = TransactionStatus::aborted;
        goto FINISH_TWRITE;
      }
#endif

      // upgrade success
      for (auto lItr = r_lock_list_.begin(); lItr != r_lock_list_.end();
           ++lItr) {
        if (*lItr == &((*rItr).rcdptr_->lock_)) {
          write_set_.emplace_back(key, (*rItr).rcdptr_);
          w_lock_list_.emplace_back(&(*rItr).rcdptr_->lock_);
          r_lock_list_.erase(lItr);
          break;
        }
      }

      read_set_.erase(rItr);
      goto FINISH_TWRITE;
    }
  }

  Tuple *tuple;
#if MASSTREE_USE
  tuple = MT.get_value(key);
#if ADD_ANALYSIS
  ++sres_->local_tree_traversal_;
#endif
#else
  tuple = get_tuple(Table, key);
#endif

#if DLR0
  tuple->lock_.w_lock();
#elif defined(DLR1)
  if (!tuple->lock_.w_trylock()) {
    this->status_ = TransactionStatus::aborted;
    goto FINISH_TWRITE;
  }
#endif

  w_lock_list_.emplace_back(&tuple->lock_);
  write_set_.emplace_back(key, tuple);

FINISH_TWRITE:
#if ADD_ANALYSIS
  sres_->local_write_latency_ += rdtscp() - start;
#endif // ADD_ANALYSIS
  return;
}

void TxExecutor::unlockList() {
  for (auto itr = r_lock_list_.begin(); itr != r_lock_list_.end(); ++itr)
    (*itr)->r_unlock();

  for (auto itr = w_lock_list_.begin(); itr != w_lock_list_.end(); ++itr)
    (*itr)->w_unlock();

  r_lock_list_.clear();
  w_lock_list_.clear();
}
