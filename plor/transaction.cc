
#include <stdio.h>
#include <string.h>
#include <vector>
#include <algorithm> 

#include <atomic>

#include "../include/backoff.hh"
#include "../include/debug.hh"
#include "../include/procedure.hh"
#include "../include/result.hh"
#include "include/common.hh"
#include "include/transaction.hh"

using namespace std;

extern void display_procedure_vector(std::vector<Procedure> &pro);

/**
 * @brief Search xxx set
 * @detail Search element of local set corresponding to given key.
 * In this prototype system, the value to be updated for each worker thread 
 * is fixed for high performance, so it is only necessary to check the key match.
 * @param Key [in] the key of key-value
 * @return Corresponding element of local set
 */
inline SetElement<Tuple> *TxExecutor::searchReadSet(uint64_t key) {
  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
    if ((*itr).key_ == key) return &(*itr);
  }

  return nullptr;
}

/**
 * @brief Search xxx set
 * @detail Search element of local set corresponding to given key.
 * In this prototype system, the value to be updated for each worker thread 
 * is fixed for high performance, so it is only necessary to check the key match.
 * @param Key [in] the key of key-value
 * @return Corresponding element of local set
 */
inline SetElement<Tuple> *TxExecutor::searchWriteSet(uint64_t key) {
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    if ((*itr).key_ == key) return &(*itr);
  }

  return nullptr;
}

/**
 * @brief function about abort.
 * Clean-up local read/write set.
 * Release locks.
 * @return void
 */
void TxExecutor::abort() {
  /**
   * Release locks
   */
  unlockList();

  /**
   * Clean-up local read/write set.
   */
  read_set_.clear();
  write_set_.clear();

  ++sres_->local_abort_counts_;

#if BACK_OFF
#if ADD_ANALYSIS
  uint64_t start(rdtscp());
#endif

  Backoff::backoff(FLAGS_clocks_per_us);

#if ADD_ANALYSIS
  sres_->local_backoff_latency_ += rdtscp() - start;
#endif

#endif
}

/**
 * @brief success termination of transaction.
 * @return void
 */
void TxExecutor::commit() {

  Tuple *tuple;
  /**
   * Phase 1: detect read-write conflicts
   */

  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    tuple = (*itr).rcdptr_;
    //tuple->waitRd.emplace_back(excl_sig);
    for (int i=0; i<tuple->waitRd.size(); i++) {
      if (thread_timestamp[this-thid_] < thread_timestamp[tuple->waitRd[i].first]) {
        thread_stats[tuple->waitRd[i].first] = 1;
      } else {
        /*
        while (1) {
          if (thread_stats[thid_] == 1) goto FINISH_WRITE;
        }
        */
      }
    }
  }

  /**
   * Phase 2: release read locks
   */
  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
    tuple = (*itr).rcdptr_;
    unlockRead(this->thid_, tuple);
    //(*itr).rcdptr_->readers[this->thid_] = -1;
  } 

  /**
   * Phase 3: commit and release write locks
   */

  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    tuple = (*itr).rcdptr_;
    // update payload.
    memcpy((*itr).rcdptr_->val_, write_val_, VAL_SIZE);
    unlockWrite(this->thid_, tuple);
    //(*itr).rcdptr_->writers[this->thid_] = -1;
  }

  //Release locks.
  //unlockList();

  /**
   * Clean-up local read/write set.
   */
  read_set_.clear();
  write_set_.clear();
}

/**
 * @brief Initialize function of transaction.
 * Allocate timestamp.
 * @return void
 */
void TxExecutor::begin() { this->status_ = TransactionStatus::inFlight; }

/**
 * @brief Transaction read function.
 * @param [in] key The key of key-value
 */
void TxExecutor::read(uint64_t key) {
#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif  // ADD_ANALYSIS

  /**
   * read-own-writes or re-read from local read set.
   */
  if (searchWriteSet(key) || searchReadSet(key)) goto FINISH_READ;

  /**
   * Search tuple from data structure.
   */
  Tuple *tuple;
#if MASSTREE_USE
  tuple = MT.get_value(key);
#if ADD_ANALYSIS
  ++sres_->local_tree_traversal_;
#endif
#else
  tuple = get_tuple(Table, key);
#endif

  /**
   * Write-Read (WR) conflict
   */

  /** PLOR */
  lockRead();

  /** Wound-Wait 
   *

  while (1) {
    if (tuple->lock_.r_trylock()) {
      r_lock_list_.emplace_back(&tuple->lock_);
      read_set_.emplace_back(key, tuple, tuple->val_);
      tuple->readers[thid_] = 1;
      break;
    }
    else {
      for (int i = 0; i < FLAGS_thread_num; i++) {
        if (tuple->writers[i] > 0 && thread_timestamp[i] > thread_timestamp[this->thid_]) {
          thread_stats[i] = 1;
        }
      }
      if (thread_stats[thid_] == 1) goto FINISH_READ;
    }
  }

  */

FINISH_READ:

#if ADD_ANALYSIS
  sres_->local_read_latency_ += rdtscp() - start;
#endif
  return;
}

/**
 * @brief transaction write operation
 * @param [in] key The key of key-value
 * @return void
 */
void TxExecutor::write(uint64_t key) {
#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif

  // if it already wrote the key object once.
  if (searchWriteSet(key)) goto FINISH_WRITE;
  /**                                                                                                                                                                
   * Search tuple from data structure.                                                                                                                               
   */
  Tuple *tuple;
#if MASSTREE_USE
  tuple = MT.get_value(key);
#if ADD_ANALYSIS
  ++sres_->local_tree_traversal_;
#endif
#else
  tuple = get_tuple(Table, key);
#endif
  /*
  for (auto rItr = read_set_.begin(); rItr != read_set_.end(); ++rItr) {
    if ((*rItr).key_ == key) {  // hit
      while (1) {
        if (!(*rItr).rcdptr_->lock_.tryupgrade()) {
          if (tuple->curr_writer > 0 && thread_timestamp[this-thid_] < thread_timestamp[tuple->curr_writer]) {
            thread_stats[tuple->curr_writer] = 1;
          }
          if (thread_stats[thid_] == 1) goto FINISH_WRITE;
        } else {
          break;
        }
      }

      // upgrade success
      // remove old element of read lock list.
      //tuple->readers[this->thid_] = 0;
      //tuple->writers[this->thid_] = 1;
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
      goto FINISH_WRITE;
    }
  }
  */

  /** PLOR */
  lockWrite();

  /** Wound-Wait 
   *
  while (1) {
    if (!tuple->lock_.w_trylock()) {
      // wound-wait
       
      for (int i = 0; i < FLAGS_thread_num; i++) {
          if ((tuple->readers[i] > 0 || tuple->writers[i] > 0) && thread_timestamp[i] > thread_timestamp[this->thid_]) {
            thread_stats[i] = 1;
          }
        }
        if (thread_stats[thid_] == 1) goto FINISH_WRITE;
      } else {
        break;
    }
  }
  */

  memcpy(tuple->val_, write_val_, VAL_SIZE);

  /**
   * Register the contents to write lock list and write set.
   */
  //tuple->writers[thid_] = 1;
  //w_lock_list_.emplace_back(&tuple->lock_);
  write_set_.emplace_back(key, tuple);

FINISH_WRITE:
#if ADD_ANALYSIS
  sres_->local_write_latency_ += rdtscp() - start;
#endif  // ADD_ANALYSIS
  return;
}

/**
 * @brief transaction readWrite (RMW) operation
 */
void TxExecutor::readWrite(uint64_t key) {
  // if it already wrote the key object once.
  if (searchWriteSet(key)) goto FINISH_WRITE;
  /**                                                                                                                                                                
   * Search tuple from data structure.                                                                                                                               
   */
  Tuple *tuple;
#if MASSTREE_USE
  tuple = MT.get_value(key);
#if ADD_ANALYSIS
  ++sres_->local_tree_traversal_;
#endif
#else
  tuple = get_tuple(Table, key);
#endif
/*
  for (auto rItr = read_set_.begin(); rItr != read_set_.end(); ++rItr) {
    if ((*rItr).key_ == key) {  // hit
      
      while (1) {
        if (!(*rItr).rcdptr_->lock_.tryupgrade()) {
          if (tuple->curr_writer > 0 && thread_timestamp[this-thid_] < thread_timestamp[tuple->curr_writer]) {
            thread_stats[tuple->curr_writer] = 1;
          }
          if (thread_stats[thid_] == 1) goto FINISH_WRITE;
        } else {
          break;
        }
      }
      

      // upgrade success
      // remove old element of read set.
      //tuple->readers[this->thid_] = 0;
      //tuple->writers[this->thid_] = 1;
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
      goto FINISH_WRITE;
    }
  }
  */

  /** PLOR */
  lockWrite();

  /** Wound-Wait 
   *
  while (1) {
    if (!tuple->lock_.w_trylock()) {
      // wound-wait.                                                                                                                                                  
      
      for (int i = 0; i < FLAGS_thread_num; i++) {
          if ((tuple->readers[i] > 0 || tuple->writers[i] > 0) && thread_timestamp[i] > thread_timestamp[this->thid_]) {
            thread_stats[i] = 1;
          }
        }
        if (thread_stats[thid_] == 1) goto FINISH_WRITE;
      } else {
        break;
    }
  }
  */

  // read payload
  memcpy(this->return_val_, tuple->val_, VAL_SIZE);
  // finish read.
  memcpy(tuple->val_, write_val_, VAL_SIZE);

  /**
   * Register the contents to write lock list and write set.
   */
  //w_lock_list_.emplace_back(&tuple->lock_);
  write_set_.emplace_back(key, tuple);

FINISH_WRITE:
  return;
}

void TxExecutor::lockRead() {
  /** PLOR */
  // @task: need to add exclusive signifier for commiting writer
  read_set_.emplace_back(key, tuple, tuple->val_);
  tuple->waitRd.emplace_back(thid_, thread_timestamp[this->thid_]);

  // pure, no commit priority yet
  while (1) {
    if (tuple->curr_writer > 0 && thread_timestamp[this-thid_] < thread_timestamp[tuple->curr_writer]) {
      thread_stats[tuple->curr_writer] = 1;
    }
    if (thread_stats[thid_] == 1) goto FINISH_READ;
  }

  /** Wound-Wait 
   *
  tuple->readers[thid_] = 1;
  for (int i = 0; i < FLAGS_thread_num; i++) {
    if (tuple->writers[i] > 0 && thread_timestamp[i] > thread_timestamp[this->thid_]) {
          thread_stats[i] = 1;
        }
  }
  */
}

void TxExecutor::lockWrite() {
  write_set_.emplace_back(key, tuple);
  tuple->waitWr.emplace_back(thid_, thread_timestamp[this->thid_]);
  sort(tuple->waitWr.begin(), tuple->waitWr.end(),[](const pair &x, const pair &y) {
    if (x.second != y.second) {
      return x.second < y.second;
    }
  }); // sort by ts

  // @task: need to use CAS for this 
  while (1) {
    if (tuple->curr_writer == 0) {
      tuple->curr_writer == this->thid_;
    } else {
      while (tuple->writer != this->thid_) {
        if (thread_timestamp[this-thid_] < thread_timestamp[tuple->curr_writer]) {
          thread_stats[tuple->curr_writer] = 1;
        }
        if (thread_stats[thid_] == 1) goto FINISH_WRITE;
      }
    }
  }
}

void TxExecutor::unlockRead(int thid, Tuple *tuple) {
  int j;
  for (int i=0; i<tuple->waitRd.size(); i++) {
    if (tuple->waitRd[i].first == thid) {
      j = i;
      break;
    }
  }
  tuple->waitRd.erase(tuple->waitRd.begin()+j);
}

void TxExecutor::unlockWrite(int thid, Tuple *tuple) {

  /* @task: 
   * tuple->waitWr.pop_front();
   * remove exclusive signifier in read set
   * turn the lock to the oldest waiter(writer) 
   */
  int j;
  for (int i=0; i<tuple->waitWr.size(); i++) {
    if (tuple->waitWr[i].first == thid_) {
      j = i;
      break;
    }
  }
  tuple->waitWr.erase(tuple->waitWr.begin()+j);
  
  if (curr_writer == thid_) {
    // remove exclusive signifier in read set
    tuple->curr_writer = tuple->waitWr[0].first;
  }
}

/**
 * @brief unlock and clean-up local lock set.
 * @return void
 */

void TxExecutor::unlockList() {
  for (auto itr = r_lock_list_.begin(); itr != r_lock_list_.end(); ++itr)
    (*itr)->r_unlock();

  for (auto itr = w_lock_list_.begin(); itr != w_lock_list_.end(); ++itr)
    (*itr)->w_unlock();

  /*
  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) 
    (*itr).rcdptr_->readers[this->thid_] = -1; 

  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) 
    (*itr).rcdptr_->writers[this->thid_] = -1;
  */

  /**
   * Clean-up local lock set.
   */
  r_lock_list_.clear();
  w_lock_list_.clear();
}

