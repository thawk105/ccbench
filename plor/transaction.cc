
#include <stdio.h>
#include <string.h>

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
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    /**
     * update payload.
     */
    memcpy((*itr).rcdptr_->val_, write_val_, VAL_SIZE);
  }

  /**
   * Release locks.
   */
  unlockList();

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
  for (auto rItr = read_set_.begin(); rItr != read_set_.end(); ++rItr) {
    if ((*rItr).key_ == key) {  // hit
      while (1) {
	if (!(*rItr).rcdptr_->lock_.tryupgrade()) {
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

      // upgrade success
      // remove old element of read lock list.
      tuple->readers[this->thid_] = 0;
      tuple->writers[this->thid_] = 1;
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

  while (1) {
    if (!tuple->lock_.w_trylock()) {
      /**
       * wound-wait
       ** Write-Write (WW) conflict
       ** Read-Write (RW) conflict
       */
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


  /**
   * Register the contents to write lock list and write set.
   */
  tuple->writers[thid_] = 1;
  w_lock_list_.emplace_back(&tuple->lock_);
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
  for (auto rItr = read_set_.begin(); rItr != read_set_.end(); ++rItr) {
    if ((*rItr).key_ == key) {  // hit
      while (1) {
        if (!(*rItr).rcdptr_->lock_.tryupgrade()) {
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

      // upgrade success
      // remove old element of read set.
      tuple->readers[this->thid_] = 0;
      tuple->writers[this->thid_] = 1;
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

  while (1) {
    if (!tuple->lock_.w_trylock()) {
      /**                                                                                                                                                             
       * wound-wait.                                                                                                                                                  
       */
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

  // read payload
  memcpy(this->return_val_, tuple->val_, VAL_SIZE);
  // finish read.

  /**
   * Register the contents to write lock list and write set.
   */
  w_lock_list_.emplace_back(&tuple->lock_);
  write_set_.emplace_back(key, tuple);

FINISH_WRITE:
  return;
}

void TxExecutor::lockRead() {
  /** PLOR */
  // @task: need to add exclusive signifier for commiting writer
  read_set_.emplace_back(key, tuple, tuple->val_);
  tuple->readers[thid_] = 1;
  for (int i = 0; i < FLAGS_thread_num; i++) {
    if (tuple->writers[i] > 0 && thread_timestamp[i] > thread_timestamp[this->thid_]) {
          thread_stats[i] = 1;
        }
  }
  if (thread_stats[thid_] == 1) goto FINISH_READ;
}

void TxExecutor::lockWrite() {
  write_set_.emplace_back(key, tuple);
  tuple->writers[thid_] = 1;
  // @task: CAS for curr_writer @then: wound-wait 
}

void TxExecutor::unlockRead() {
  tuple->readers[thid_] = 0;
  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) 
    (*itr).rcdptr_->readers[this->thid_] = -1; 
}

void TxExecutor::unlockWrite() {
  tuple->writers[thid_] = 0;
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) 
    (*itr).rcdptr_->writers[this->thid_] = -1;
  /* @task: turn the lock to the oldest waiter(writer)
   * curr_writer = smallestTimestamp(for (int i = 0; i < FLAGS_thread_num; i++) {writers[i]>0})
   */ 
}

/**
 * @brief unlock and clean-up local lock set.
 * @return void
 */

void TxExecutor::unlockList() {
  for (auto itr = r_lock_list_.begin(); itr != r_lock_list_.end(); ++itr)
    (*itr)->r_unlock();

  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) 
    (*itr).rcdptr_->readers[this->thid_] = -1; 

  for (auto itr = w_lock_list_.begin(); itr != w_lock_list_.end(); ++itr)
    (*itr)->w_unlock();

  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) 
    (*itr).rcdptr_->writers[this->thid_] = -1;

  /**
   * Clean-up local lock set.
   */
  r_lock_list_.clear();
  w_lock_list_.clear();
}

