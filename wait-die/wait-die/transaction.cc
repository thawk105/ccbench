    
#include <stdio.h>
#include <string.h>

#include <atomic>

#include "../include/backoff.hh"
#include "../include/debug.hh"
#include "../include/procedure.hh"
#include "../include/result.hh"
#include "include/common.hh"
#include "include/transaction.hh"

// #define NONTS

using namespace std;

extern void display_procedure_vector(std::vector<Procedure> &pro);

void TxExecutor::warmupTuple(uint64_t key) {
  Tuple *tuple;
#if MASSTREE_USE
  tuple = MT.get_value(key);
#else
  tuple = get_tuple(Table, key);
#endif
  // memcpy(tuple->val_, write_val_, VAL_SIZE);
  tuple->lock_.init();
  tuple->latch_.init();
  tuple->writer_ = UNLOCKED;
  tuple->reader_ = UNLOCKED;
  tuple->writeflag_ = false;
}

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
  //printf("tx%d commit\n", thid_);
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
#ifdef NONTS
  txid_ += FLAGS_thread_num;
#endif
}

/**
 * @brief Initialize function of transaction.
 * Allocate timestamp.
 * @return void
 */
void TxExecutor::begin() { this->status_ = TransactionStatus::inFlight; }

void setOwner(atomic<int> &owner, int txid){
  int expected, desired;
  desired = txid;
  for (;;) {
    expected = owner.load(memory_order_acquire);
    if (owner.compare_exchange_strong(
            expected, desired, memory_order_acq_rel, memory_order_acquire))
      return;
  }
}

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
  while(1) {
      if(!tuple->latch_.w_trylock()) {
        usleep(1);
        continue;
      }
      if(tuple->writeflag_) {
          if(txid_ > tuple->writer_) {
              tuple->latch_.w_unlock();
              status_ = TransactionStatus::aborted;
              goto FINISH_READ; 
          }
          tuple->latch_.w_unlock();
          usleep(1);
          continue;
      }
      else {
          while(1) {
              if(tuple->lock_.r_trylock()) {
                if(txid_ < tuple->reader_)
                  tuple->reader_ = txid_;
                tuple->latch_.w_unlock();
                read_set_.emplace_back(key, tuple, tuple->val_);
                goto FINISH_READ;
              }
              usleep(1);
          }
      }
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
  // cout << "tuple " << key << endl;
  // cout << "tuple reader_ = " << tuple->reader_ << endl;
  // cout << "tuple writer_ = " << tuple->writer_ << endl;
  for (auto rItr = read_set_.begin(); rItr != read_set_.end(); ++rItr) {
    if ((*rItr).key_ == key) {  // hit
      while(1) {
        if(!tuple->latch_.w_trylock()) {
          usleep(1);
          continue;
        }
        if(tuple->writeflag_) {
          if(txid_ > tuple->writer_) {
            tuple->latch_.w_unlock();
            status_ = TransactionStatus::aborted;
            goto FINISH_WRITE;
          }
          tuple->latch_.w_unlock();
          usleep(1);
          continue;
        }
        tuple->writeflag_ = true; // the fact that i am holding the lock suggests that there is no need to spinwait writeflag
        tuple->writer_ = txid_;
        if(txid_ > tuple->reader_) {
          tuple->writeflag_ = false;
          tuple->latch_.w_unlock();
          status_ = TransactionStatus::aborted;
          goto FINISH_WRITE;
        }
        tuple->latch_.w_unlock();
        while(1) { 
          if(tuple->lock_.tryupgrade()) {
            tuple->reader_ = UNLOCKED;
            tuple->writer_ = txid_;
            write_set_.emplace_back(key, (*rItr).rcdptr_);
            read_set_.erase(rItr);
            goto FINISH_WRITE;
          }
          if(txid_ > tuple->reader_) {
            tuple->writeflag_ = false;
            status_ = TransactionStatus::aborted;
            goto FINISH_WRITE;
          }
          usleep(1);
        }
      }
    }
  }

  while (1) {
    if(!tuple->latch_.w_trylock()) {
      usleep(1);
      continue;
    }
    if(tuple->writeflag_) {
      if(txid_ > tuple->writer_) {
        tuple->latch_.w_unlock();
        status_ = TransactionStatus::aborted;
        goto FINISH_WRITE;
      }
      tuple->latch_.w_unlock();
      usleep(1);
      continue;
    }
    tuple->writeflag_ = true;
    tuple->writer_ = txid_;
    if(txid_ > tuple->reader_) {
        tuple->writeflag_ = false;
        tuple->latch_.w_unlock();
        status_ = TransactionStatus::aborted;
        goto FINISH_WRITE;
    }
    tuple->latch_.w_unlock();
    while(1){
        if(tuple->lock_.w_trylock()) {
          tuple->writer_ = txid_;
          write_set_.emplace_back(key, tuple);
          goto FINISH_WRITE;
        }
        if(txid_ > tuple->reader_) {
          tuple->writeflag_ = false;
            status_ = TransactionStatus::aborted;
            goto FINISH_WRITE;
        }
        usleep(1);
    }
  }

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
  Tuple *tuple;
  tuple = get_tuple(Table, key);
  for (auto rItr = read_set_.begin(); rItr != read_set_.end(); ++rItr) {
    if ((*rItr).key_ == key) {  // hit
      tuple->latch_.w_lock();
      if(txid_ > tuple->reader_) {
        tuple->latch_.w_unlock();
        status_ = TransactionStatus::aborted;
        goto FINISH_WRITE;
      }
      else {
        tuple->writeflag_ = true; // the fact that i am holding the lock suggests that there is no need to spinwait writeflag
        tuple->latch_.w_unlock();
        while(1) { 
          if(tuple->lock_.tryupgrade()) {
            tuple->reader_ = UNLOCKED;
            tuple->writer_ = txid_;
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
          usleep(1);
        }
      }
    }
  }

  while (1) {
    tuple->latch_.w_lock();
    if(tuple->writeflag_) {
      if(txid_ > tuple->writer_) {
        status_ = TransactionStatus::aborted;
        tuple->latch_.w_unlock();
        goto FINISH_WRITE;
      }
      tuple->latch_.w_unlock();
      usleep(1);
      continue;
    }
    tuple->writeflag_ = true;
    if(txid_ > tuple->reader_) {
        tuple->writeflag_ = false;
        tuple->latch_.w_unlock();
        status_ = TransactionStatus::aborted;
        goto FINISH_WRITE;
    }
    tuple->latch_.w_unlock();
    while(1){
        if(tuple->lock_.w_trylock()) {
          tuple->latch_.w_unlock();
          tuple->writer_ = txid_;
          memcpy(this->return_val_, tuple->val_, VAL_SIZE);
          w_lock_list_.emplace_back(&tuple->lock_);
          write_set_.emplace_back(key, tuple);
          goto FINISH_WRITE;
        }
        if(txid_ > tuple->reader_) {
            status_ = TransactionStatus::aborted;
            goto FINISH_WRITE;
        }
        usleep(1);
    }
  }

FINISH_WRITE:
  return;
}

/**
 * @brief unlock and clean-up local lock set.
 * @return void
 */
void TxExecutor::unlockList() {

  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
    (*itr).rcdptr_->latch_.w_lock();
    int reader = (*itr).rcdptr_->lock_.counter.load(memory_order_acquire);
    if(reader == 1) (*itr).rcdptr_->reader_ = UNLOCKED;
    (*itr).rcdptr_->lock_.r_unlock();
    (*itr).rcdptr_->latch_.w_unlock();
  }

  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    (*itr).rcdptr_->writeflag_ = false;
    (*itr).rcdptr_->writer_ = UNLOCKED;
    (*itr).rcdptr_->lock_.w_unlock();
  }

  /**
   * Clean-up local lock set.
   */
  r_lock_list_.clear();
  w_lock_list_.clear();
}
