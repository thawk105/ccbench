
#include <stdio.h>
#include <string.h>
#include <utility>
#include <vector>
#include <algorithm> 

#include <atomic>
#include <mutex>

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
  
  for (auto itr = r_lock_list_.begin(); itr != r_lock_list_.end(); ++itr)
    (*itr)->r_unlock();

  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
    unlockRead(this->thid_, (*itr).rcdptr_);
  } 
  
  for (auto itr = w_lock_list_.begin(); itr != w_lock_list_.end(); ++itr)
    (*itr)->w_unlock();
    
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    unlockWrite(this->thid_, (*itr).rcdptr_);
  }

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

// change to validate
bool TxExecutor::validationPhase() {
#if ADD_ANALYSIS
  std::uint64_t start = rdtscp();
#endif


  if (thread_stats[this->thid_] == 1) {
    return false;
  }
  /**
  * Phase 1: detect read-write conflicts
  */
  
  //wait<pair<ts, thid>>
  NNN;
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    (*itr).rcdptr_->waitRd.emplace_back(-1,-1);
    for (int i=0; i<(*itr).rcdptr_->waitRd.size(); i++) {
      if (thread_timestamp[this->thid_] < (*itr).rcdptr_->waitRd[i].first) {
        thread_stats[(*itr).rcdptr_->waitRd[i].second] = 1;
      } else {
        // checkRd
        NNN;
        while (checkRd((*itr).rcdptr_->waitRd[i].second, (*itr).rcdptr_)) {
          NNN;
          if (thread_stats[this->thid_] == 1) {
            break;
          }
          return false;
        }
      }
    }
  }
  
  // goto Phase 3
#if ADD_ANALYSIS
  sres_->local_vali_latency_ += rdtscp() - start;
#endif
  this->status_ = TransactionStatus::committed;
  NNN;
  return true;
}

void TxExecutor::commit() {
  /**
   * Phase 2: release read locks
   */

  for (auto itr = r_lock_list_.begin(); itr != r_lock_list_.end(); ++itr)
    (*itr)->r_unlock();

  NNN;
  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
    unlockRead(this->thid_, (*itr).rcdptr_);
  } 

  /**
   * Phase 3: commit and release write locks
   */
  
  for (auto itr = w_lock_list_.begin(); itr != w_lock_list_.end(); ++itr)
    (*itr)->w_unlock();

  NNN;
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    // update payload.
    memcpy((*itr).rcdptr_->val_, write_val_, VAL_SIZE);
    unlockWrite(this->thid_, (*itr).rcdptr_);
  }

  NNN;
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

  /**
   * @c task: check readlockAcquire bamboo
   */
  /** PLOR */
  /*
  tuple->waitRd.emplace_back(thread_timestamp[this->thid_], this->thid_);
  while (checkRd(-1, tuple)) {
    if (tuple->curr_writer > 0 && thread_timestamp[this->thid_] < thread_timestamp[tuple->curr_writer]) {
      thread_stats[tuple->curr_writer] = 1;
    }
    if (thread_stats[this->thid_] == 1) goto FINISH_READ;
  }
  read_set_.emplace_back(key, tuple, tuple->val_);
  */
  NNN;
   while (1) {
    if (tuple->lock_.r_trylock()) {
      r_lock_list_.emplace_back(&tuple->lock_);
      read_set_.emplace_back(key, tuple, tuple->val_);
      break;
    }
    else {
      tuple->waitRd.emplace_back(thread_timestamp[this->thid_], this->thid_);
      while (checkRd(-1, tuple)) {
        if (tuple->curr_writer > 0 && thread_timestamp[this->thid_] < thread_timestamp[tuple->curr_writer]) {
          thread_stats[tuple->curr_writer] = 1;
        }
        if (thread_stats[this->thid_] == 1) goto FINISH_READ;
      }
      read_set_.emplace_back(key, tuple, tuple->val_);
    }
  }

  NNN;
  r_lock_list_.emplace_back(&tuple->lock_);
  read_set_.emplace_back(key, tuple, tuple->val_);

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

  /** PLOR */
  /*
  tuple->waitWr.emplace_back(thread_timestamp[this->thid_],this->thid_);
  sort(tuple->waitWr.begin(), tuple->waitWr.end());
  int expected, desired;
  expected = 0;
  desired = this->thid_;
  if (!tuple->curr_writer.compare_exchange_weak(expected,desired)) {
    while (tuple->curr_writer != this->thid_) {
      if (thread_timestamp[this->thid_] < thread_timestamp[tuple->curr_writer]) {
        thread_stats[tuple->curr_writer] = 1;
      }
      if (thread_stats[this->thid_] == 1) goto FINISH_WRITE;
    }
  }  
  */

  for (auto rItr = read_set_.begin(); rItr != read_set_.end(); ++rItr) {
    if ((*rItr).key_ == key) {  // hit
      while (1) {
        if (!(*rItr).rcdptr_->lock_.tryupgrade()) {
          if (thread_timestamp[this->thid_] < thread_timestamp[tuple->curr_writer]) {
            thread_stats[tuple->curr_writer] = 1;
          }
          if (thread_stats[this->thid_] == 1) goto FINISH_WRITE;
        } else {
          break;
        }
      }

      // upgrade success
      // remove old element of read lock list.
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

  NNN;
  while (1) {
    if (!tuple->lock_.w_trylock()) {
      tuple->waitWr.emplace_back(thread_timestamp[this->thid_],this->thid_);
      sort(tuple->waitWr.begin(), tuple->waitWr.end());
      int expected, desired;
      expected = 0;
      desired = this->thid_;
      if (!tuple->curr_writer.compare_exchange_weak(expected,desired)) {
        while (tuple->curr_writer != this->thid_) {
          if (thread_timestamp[this->thid_] < thread_timestamp[tuple->curr_writer]) {
            thread_stats[tuple->curr_writer] = 1;
          }
          if (thread_stats[this->thid_] == 1) goto FINISH_WRITE;
        }
      }
    } else {
      break;
    }
  }


  /**
   * Register the contents to write lock list and write set.
   */
   NNN;
  memcpy(tuple->val_, write_val_, VAL_SIZE);
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

  /** PLOR */
  /*
  tuple->waitWr.emplace_back(thread_timestamp[this->thid_], this->thid_);
  sort(tuple->waitWr.begin(), tuple->waitWr.end());
  int expected, desired;
  expected = 0;
  desired = this->thid_;
  
  if (!tuple->curr_writer.compare_exchange_weak(expected,desired)) {
    while (tuple->curr_writer != this->thid_) {
      if (thread_timestamp[this->thid_] < thread_timestamp[tuple->curr_writer]) {
        thread_stats[tuple->curr_writer] = 1;
      }
      if (thread_stats[this->thid_] == 1) goto FINISH_WRITE;
    }
  }
  */

  for (auto rItr = read_set_.begin(); rItr != read_set_.end(); ++rItr) {
    if ((*rItr).key_ == key) {  // hit
      while (1) {
        if (!(*rItr).rcdptr_->lock_.tryupgrade()) {
          if (thread_timestamp[this->thid_] < thread_timestamp[tuple->curr_writer]) {
            thread_stats[tuple->curr_writer] = 1;
          }
          if (thread_stats[this->thid_] == 1) goto FINISH_WRITE;
        } else {
          break;
        }
      }

      // upgrade success
      // remove old element of read lock list.
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
      tuple->waitWr.emplace_back(thread_timestamp[this->thid_],this->thid_);
      sort(tuple->waitWr.begin(), tuple->waitWr.end());
      int expected, desired;
      expected = 0;
      desired = this->thid_;
      if (!tuple->curr_writer.compare_exchange_weak(expected,desired)) {
        while (tuple->curr_writer != this->thid_) {
          if (thread_timestamp[this->thid_] < thread_timestamp[tuple->curr_writer]) {
            thread_stats[tuple->curr_writer] = 1;
          }
          if (thread_stats[this->thid_] == 1) goto FINISH_WRITE;
        }
      }
    } else {
      break;
    }
  }


  /**
   * Register the contents to write lock list and write set.
   */
  // read payload
  memcpy(this->return_val_, tuple->val_, VAL_SIZE);
  // finish read.
  memcpy(tuple->val_, write_val_, VAL_SIZE);
  w_lock_list_.emplace_back(&tuple->lock_);
  write_set_.emplace_back(key, tuple);

FINISH_WRITE:
  return;
}

void TxExecutor::unlockRead(int thid, Tuple *tuple) {
  mtx_get();
  int j;
  NNN;
  for (int i=0; i<tuple->waitRd.size(); i++) {
    if (tuple->waitRd[i].second == thid) {
      j = i;
      break;
    }
  }
  tuple->waitRd.erase(tuple->waitRd.begin()+j); //THIS IS THE BUG
  mtx_release();
}

void TxExecutor::unlockWrite(int thid, Tuple *tuple) {
  mtx_get();
  /* @task: 
   * tuple->waitWr.pop_front();
   * remove exclusive signifier in read set
   * turn the lock to the oldest waiter(writer) 
   */
  int j, k;
  NNN;
  for (int i=0; i<tuple->waitWr.size(); i++) {
    if (tuple->waitWr[i].second == thid) {
      j = i;
      break;
    }
  }
  tuple->waitWr.erase(tuple->waitWr.begin()+j);
  
  NNN;
  if (tuple->curr_writer == thid) {
    // remove exclusive signifier in read set
    for (int i=0; i<tuple->waitRd.size(); i++) {
      if (tuple->waitRd[i].second == -1) {
        k = i;
        break;
      }
    }
    tuple->waitRd.erase(tuple->waitRd.begin()+k);
    tuple->curr_writer = tuple->waitWr[0].second;
  }
  mtx_release();
}

bool TxExecutor::checkRd(int thid, Tuple *tuple) {
  mtx_get();
  for (int i = 0; i<tuple->waitRd.size(); i++) {
		if (tuple->waitRd[i].second == thid) {
			return true;
		}
	}
	return false;
  mtx_release();
}

void TxExecutor::mtx_get() {
  mtx.lock();
}
    
void TxExecutor::mtx_release() {
  mtx.unlock();
}

void TxExecutor::unlockList() {
  for (auto itr = r_lock_list_.begin(); itr != r_lock_list_.end(); ++itr)
    (*itr)->r_unlock();

  for (auto itr = w_lock_list_.begin(); itr != w_lock_list_.end(); ++itr)
    (*itr)->w_unlock();

  /**
   * Clean-up local lock set.
   */
  r_lock_list_.clear();
  w_lock_list_.clear();
}