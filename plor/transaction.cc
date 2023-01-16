
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
  //unlockList();

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
  Tuple *tuple;
#if ADD_ANALYSIS
  std::uint64_t start = rdtscp();
#endif

  /**
   * Phase 1: detect read-write conflicts
   */
  NNN;
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    tuple = (*itr).rcdptr_;
    tuple->waitRd.emplace_back(-1,-1);
    for (int i=0; i<tuple->waitRd.size(); i++) {
      if (thread_timestamp[this->thid_] < thread_timestamp[tuple->waitRd[i].second]) {
        thread_stats[tuple->waitRd[i].second] = 1;
      } else {
        // checkRd
        while (checkRd(tuple->waitRd[i].second, tuple)) {
          if (thread_stats[this->thid_] == 1) {
            this->status_ = TransactionStatus::aborted;
            return false;
          }
        }
      }
    }
  }

  NNN;
  // goto Phase 3
#if ADD_ANALYSIS
  sres_->local_vali_latency_ += rdtscp() - start;
#endif
  this->status_ = TransactionStatus::committed;
  return true;
}

void TxExecutor::commit() {
  Tuple *tuple;
  /**
   * Phase 2: release read locks
   */
  NNN;
  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
    tuple = (*itr).rcdptr_;
    unlockRead(this->thid_, tuple);
    //(*itr).rcdptr_->readers[this->thid_] = -1;
  } 

  /**
   * Phase 3: commit and release write locks
   */
  NNN;
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    tuple = (*itr).rcdptr_;
    // update payload.
    memcpy((*itr).rcdptr_->val_, write_val_, VAL_SIZE);
    unlockWrite(this->thid_, tuple);
    //(*itr).rcdptr_->writers[this->thid_] = -1;
  }
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
  read_set_.emplace_back(key, tuple, tuple->val_);
  tuple->waitRd.emplace_back(thread_timestamp[this->thid_], this->thid_);

  // pure, no commit priority yet
  // need checkRd
  while (checkRd(-1, tuple)) {
    if (tuple->curr_writer > 0 && thread_timestamp[this->thid_] < thread_timestamp[tuple->curr_writer]) {
      thread_stats[tuple->curr_writer] = 1;
    }
    if (thread_stats[this->thid_] == 1) goto FINISH_READ;
  }

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
  write_set_.emplace_back(key, tuple);
  tuple->waitWr.emplace_back(thread_timestamp[this->thid_],this->thid_);
  sort(tuple->waitWr.begin(), tuple->waitWr.end());

  // @task: need to use CAS for this 
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

  /** PLOR */
  write_set_.emplace_back(key, tuple);
  tuple->waitWr.emplace_back(thread_timestamp[this->thid_], this->thid_);
  sort(tuple->waitWr.begin(), tuple->waitWr.end());

  // @task: need to use CAS for this 
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

void TxExecutor::unlockRead(int thid, Tuple *tuple) {
  int j;
  for (int i=0; i<tuple->waitRd.size(); i++) {
    if (tuple->waitRd[i].second == thid) {
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
    if (tuple->waitWr[i].second == thid) {
      j = i;
      break;
    }
  }
  tuple->waitWr.erase(tuple->waitWr.begin()+j);
  
  if (tuple->curr_writer == thid) {
    // remove exclusive signifier in read set
    tuple->curr_writer = tuple->waitWr[0].second;
  }
}

bool TxExecutor::checkRd(int thid, Tuple *tuple) {
  for (int i = 0; i<tuple->waitRd.size(); i++) {
		if (tuple->waitRd[i].second == thid) {
			return true;
		}
	}
	return false;
}

void TxExecutor::mtx_get() {
  mtx.lock();
}
    
void TxExecutor::mtx_release() {
  mtx.unlock();
}
