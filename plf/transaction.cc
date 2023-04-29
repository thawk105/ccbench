#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <atomic>

#include "../include/backoff.hh"
#include "../include/debug.hh"
#include "../include/procedure.hh"
#include "../include/result.hh"
#include "include/common.hh"
#include "include/transaction.hh"

using namespace std;

int binarySearch(bool is_insert, vector<int> &x, int goal, int tail) {
  int head = 0;
  tail = tail - 1;
  while (1) {
    int search_key = floor((head + tail) / 2);
    if (x[search_key] == goal) {
      if (is_insert) {
        exit(1);
      } else {
        return search_key;
      }
    }
    else if (goal > x[search_key]) {
      head = search_key + 1;
    }
    else if (goal < x[search_key]) {
      tail = search_key - 1;
    }
    if (tail < head) {
      if (is_insert) {
        return head;
      } else {
        return -1;
      }
    }
  }
}

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
    if ((*itr).key_ == key)
      return &(*itr);
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
    if ((*itr).key_ == key)
      return &(*itr);
  }

  return nullptr;
}

void TxExecutor::abort() {
  unlockList(false);
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
  validationPhase();
  unlockList(true);
  read_set_.clear();
  write_set_.clear();
  txid_ += FLAGS_thread_num;
}

void TxExecutor::validationPhase() {
  Tuple *tuple;
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    tuple = (*itr).rcdptr_;
    if (tuple->req_type[thid_] == 0) {
      continue;
    } else if (tuple->req_type[thid_] == -1) {
      checkWound(tuple->owners, LockType::EX, tuple);
    }
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
#endif // ADD_ANALYSIS

  /**
   * read-own-writes or re-read from local read set.
   */
  if (searchWriteSet(key) || searchReadSet(key))
    goto FINISH_READ;

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
  if (readlockAcquire(LockType::SH, key, tuple)) goto FINISH_READ;
  spinWait(key, tuple);

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
  if (searchWriteSet(key)) {
    goto FINISH_WRITE;
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

  for (auto rItr = read_set_.begin(); rItr != read_set_.end(); ++rItr) {
    if ((*rItr).key_ == key) {
      if (lockUpgrade(key, tuple)) {
        // upgrade success
        // remove old element of read lock list.
        if (spinWait(key, tuple)) {
          read_set_.erase(rItr);
          memcpy(tuple->val_, write_val_, VAL_SIZE);
        }
      }
      goto FINISH_WRITE;
    }
  }

  writelockAcquire(LockType::EX, key, tuple);
  if (spinWait(key, tuple)) {
    memcpy(tuple->val_, write_val_, VAL_SIZE);
  }


FINISH_WRITE:
#if ADD_ANALYSIS
  sres_->local_write_latency_ += rdtscp() - start;
#endif // ADD_ANALYSIS
  return;
}

/**
 * @brief transaction readWrite (RMW) operation
 */
void TxExecutor::readWrite(uint64_t key) {
  // if it already wrote the key object once.
  if (searchWriteSet(key)) {
    goto FINISH_WRITE;
  }
#ifdef NOUPGRADE
  if (searchReadSet(key)) {
    goto FINISH_WRITE;
  }
#endif
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
    if ((*rItr).key_ == key) {
      if (lockUpgrade(key, tuple)) {
        // upgrade success
        // remove old element of read lock list.
        if (spinWait(key, tuple)) {
          read_set_.erase(rItr);
          memcpy(tuple->val_, write_val_, VAL_SIZE);
          
        }
      }
      goto FINISH_WRITE;
    }
  }
  /**
   * Search tuple from data structure.
   */
  writelockAcquire(LockType::EX, key, tuple);
  if (spinWait(key, tuple)) {
    // read payload
    memcpy(this->return_val_, tuple->val_, VAL_SIZE);
    // finish read.
    memcpy(tuple->val_, write_val_, VAL_SIZE);
    
  }

FINISH_WRITE:
  return;
}

/**
 * @brief unlock and clean-up local lock set.
 * @return void
 */
void TxExecutor::unlockList(bool is_commit) {
  Tuple *tuple;
  bool shouldRollback;
  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
    tuple = (*itr).rcdptr_;
    LockRelease((*itr).key_, tuple);
  }
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    tuple = (*itr).rcdptr_;
    if (tuple->req_type[thid_] == 0) {
      continue;
    }
    LockRelease((*itr).key_, tuple);
    //mtx_get();
    //committing = false;
   //mtx_release();
    if (is_commit) 
      memcpy(tuple->val_, (*itr).val_, VAL_SIZE); 
  }
}

bool TxExecutor::conflict(LockType x, LockType y) {
  if (((x == LockType::EX) && (y == LockType::SH)) || ((x == LockType::SH) && (y == LockType::EX)))
    return true;
  else
    return false;
}

void TxExecutor::PromoteWaiters(Tuple *tuple) {
  int t, tThread;
  int owner, ownerThread;
  LockType t_type;
  LockType owners_type;
  bool owner_exists = false;

  int r, rThread;

  while (tuple->waiters.size()) {
    if (tuple->owners.size()) {
      owner = tuple->owners[0];
      ownerThread = owner % FLAGS_thread_num;
      owners_type = (LockType)tuple->req_type[ownerThread];
      owner_exists = true;
    }
    t = tuple->waiters[0];
    tThread = t % FLAGS_thread_num;
    t_type = (LockType)tuple->req_type[tThread];
    if (owner_exists && conflict(t_type, owners_type)) {
      break;
    }
    tuple->remove(t, tuple->waiters);
    tuple->ownersAdd(t);
  }
}

void TxExecutor::checkWound(vector<int> &list, LockType lock_type, Tuple *tuple) {
  int t, tThread;
  bool has_conflicts = false;
  LockType type;
  for (auto it = list.begin(); it != list.end();) {
    t = (*it);
    tThread = t % FLAGS_thread_num;
    type = (LockType)tuple->req_type[tThread];
    has_conflicts = false;
    if (txid_ != t && conflict(lock_type, type)) {
      has_conflicts = true;
    }
    if (has_conflicts == true && txid_ < t) {
      TxPointers[tThread]->status_ = TransactionStatus::aborted;
      it = woundRelease(t, tuple);
    }
    else {
      ++it;
    }
  }
}

void TxExecutor::writelockAcquire(LockType EX_lock, uint64_t key, Tuple *tuple) {
  tuple->req_type[thid_] = EX_lock;
  while (1) {
    if (tuple->lock_.w_trylock()) {
      checkWound(tuple->owners, EX_lock, tuple);
      tuple->sortAdd(txid_, tuple->waiters);
      PromoteWaiters(tuple);
      tuple->lock_.w_unlock();
      return;
    }
    usleep(1);
  }
}


vector<int>::iterator TxExecutor::woundRelease(int txn, Tuple *tuple) {
  bool was_head = false;
  int txnThread = txn % FLAGS_thread_num;
  LockType type = (LockType)tuple->req_type[txnThread];
  int head, headThread;
  LockType head_type;

  auto it = tuple->itrRemove(txn);
  tuple->req_type[txnThread] = 0;
  return it;
}

bool TxExecutor::LockRelease(uint64_t key, Tuple *tuple) {
  bool was_head = false;
  LockType type = (LockType)tuple->req_type[thid_];
  int head, headThread;
  LockType head_type;
  while (1) {
    if (tuple->lock_.w_trylock()) {
      if (tuple->req_type[thid_] == 0) {
        tuple->lock_.w_unlock();
        return false;
      }
      
      if (tuple->ownersRemove(txid_) == false) {
        printf("REMOVE FAILURE: LockRelease tx%d\n", txid_);
        exit(1);
      }
      tuple->req_type[thid_] = 0;
      PromoteWaiters(tuple);
      tuple->lock_.w_unlock();
      return true;
    }
    if (tuple->req_type[thid_] == 0)
      return false;
    usleep(1);

  }
}

vector<int>::iterator Tuple::itrRemove(int txn) {
  vector<int>::iterator it;
  int i;
  for (i = 0; i < owners.size(); i++) {
    if (txn == owners[i]) {
      it = owners.erase(owners.begin() + i);
      return it;
    }
  }
  printf("ERROR: itrRemove FAILURE\n");
  exit(1);
}

bool Tuple::ownersRemove(int txn) {
  for (int i = 0; i < owners.size(); i++) {
    if (txn == owners[i]) {
      owners.erase(owners.begin() + i);
      return true;
    }
  }
  return false;
}

bool Tuple::remove(int txn, vector<int> &list) {
  if (list.size() == 0)
    return false;
  int i = binarySearch(false, list, txn, list.size());
  if (i == -1)
    return false;
  assert(txn == *(list.begin() + i));
  list.erase(list.begin() + i);
  return true;
}

bool Tuple::sortAdd(int txn, vector<int> &list) {
  if (list.size() == 0) {
    list.emplace_back(txn);
    return true;
  }
  int i = binarySearch(true, list, txn, list.size());
  list.insert(list.begin() + i, txn);
  return true;
}

bool TxExecutor::spinWait(uint64_t key, Tuple *tuple) {
  while (1) {
    if (tuple->lock_.w_trylock()) {
      for (int i = 0; i < tuple->owners.size(); i++) {
        if (txid_ == tuple->owners[i]) {

          if (tuple->req_type[thid_] == -1)
          {
            read_set_.emplace_back(key, tuple, tuple->val_);
          }
          else if (tuple->req_type[thid_] == 1) {
            write_set_.emplace_back(key, tuple, tuple->val_);
          }
          tuple->lock_.w_unlock();
          return true;
        }
      }
      if (status_ == TransactionStatus::aborted) {
        tuple->req_type[thid_] = 0;
        if (tuple->remove(txid_, tuple->waiters)) {
          PromoteWaiters(tuple);
          tuple->lock_.w_unlock();
          return false;
        }
        tuple->ownersRemove(txid_);
        PromoteWaiters(tuple);
        tuple->lock_.w_unlock();
        return false;
      }
      tuple->lock_.w_unlock();
    }
    usleep(1);
  }
}

bool TxExecutor::lockUpgrade(uint64_t key, Tuple *tuple) {
  const LockType my_type = LockType::SH;
  int i;

  int r, rThread;
  while (1) {
    if (tuple->lock_.w_trylock()) {
      checkWound(tuple->owners, LockType::EX, tuple);
        if (tuple->owners.size() == 1 && tuple->owners[0] == txid_) {
          tuple->req_type[thid_] = LockType::EX;
          tuple->lock_.w_unlock();
          return true;
        }

      if (status_ == TransactionStatus::aborted) {
        tuple->lock_.w_unlock();
        return false;
      }
      tuple->lock_.w_unlock();
      usleep(1);
    }
    usleep(1);

  }
}

bool TxExecutor::readlockAcquire(LockType SH_lock, uint64_t key, Tuple *tuple) {
  tuple->req_type[thid_] = SH_lock;
  bool is_retired = false;
  while (1) {
    if (tuple->lock_.w_trylock()) {
      //if (tuple->req_type[thid_] == 1) {
        checkWound(tuple->owners, SH_lock, tuple);
      //} 
      tuple->sortAdd(txid_, tuple->waiters);
      PromoteWaiters(tuple);
      tuple->lock_.w_unlock();
      return is_retired;
    }
    usleep(1);
  }
}

void TxExecutor::mtx_get() {
  mtx.lock();
}
    
void TxExecutor::mtx_release() {
  mtx.unlock();
}

bool Tuple::ownersCheck(int txn) {
  for (int i = 0; i < owners.size(); i++) {
    if (txn == owners[i]) {
      return true;
    }
  }
  return false;
}