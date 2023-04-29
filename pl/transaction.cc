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
    if (thread_timestamp[x[search_key]] == thread_timestamp[goal]) {
      return search_key;
    }
    else if (thread_timestamp[goal] > thread_timestamp[x[search_key]]) {
      head = search_key + 1;
    }
    else if (thread_timestamp[goal] < thread_timestamp[x[search_key]]) {
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

inline SetElement<Tuple> *TxExecutor::searchReadSet(uint64_t key) {
  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
    if ((*itr).key_ == key)
      return &(*itr);
  }
  return nullptr;
}

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

void TxExecutor::commit() {
  detectionPhase();
  unlockList(true);
  read_set_.clear();
  write_set_.clear();
}

void TxExecutor::detectionPhase() {
  Tuple *tuple;
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    tuple = (*itr).rcdptr_;
    mtx_get();
    tuple->committing = true;
    mtx_release();
    if (tuple->req_type[thid_] == 0) {
      continue;
    } else if (tuple->req_type[thid_] == -1) {
      letsWound(tuple->currentwriters, LockType::EX, tuple);
    }
  }
}

void TxExecutor::begin() { this->status_ = TransactionStatus::inFlight;}

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
  if (LockRd(LockType::SH, tuple)) goto FINISH_READ;
  letsWait(key, tuple);

FINISH_READ:
#if ADD_ANALYSIS
  sres_->local_read_latency_ += rdtscp() - start;
#endif
  return;
}

void TxExecutor::write(uint64_t key) {
#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif
 
  if (searchWriteSet(key)) goto FINISH_WRITE;
  
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
      if (LockUpgr(key, tuple)) {
        if (letsWait(key, tuple)) {
          read_set_.erase(rItr);
          memcpy(tuple->val_, write_val_, VAL_SIZE);
        }
      }
      goto FINISH_WRITE;
    }
  }

  LockWr(LockType::EX, tuple);
  if (letsWait(key, tuple)) {
    memcpy(tuple->val_, write_val_, VAL_SIZE);
  }

FINISH_WRITE:
#if ADD_ANALYSIS
  sres_->local_write_latency_ += rdtscp() - start;
#endif // ADD_ANALYSIS
  return;
}

void TxExecutor::readWrite(uint64_t key) {

  if (searchWriteSet(key)) goto FINISH_WRITE;

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
      if (LockUpgr(key, tuple)) {
        if (letsWait(key, tuple)) {
          read_set_.erase(rItr);
          memcpy(tuple->val_, write_val_, VAL_SIZE);
        }
      }
      goto FINISH_WRITE;
    }
  }

  LockWr(LockType::EX, tuple);
  if (letsWait(key, tuple)) {
    // read payload
    memcpy(this->return_val_, tuple->val_, VAL_SIZE);
    // finish read.
    memcpy(tuple->val_, write_val_, VAL_SIZE);
  }

FINISH_WRITE:
  return;
}

void TxExecutor::unlockList(bool is_commit)
{
  Tuple *tuple;
  bool shouldRollback;
  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
    tuple = (*itr).rcdptr_;
    Unlock((*itr).key_, tuple);
  }
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    tuple = (*itr).rcdptr_;
    if (tuple->req_type[thid_] == 0) {
      continue;
    }
    Unlock((*itr).key_, tuple);
    if (is_commit) {
      memcpy(tuple->val_, (*itr).val_, VAL_SIZE);
    }
    mtx_get();
    tuple->committing = false;
    mtx_release();
  }
}

bool TxExecutor::conflict(LockType x, LockType y) {
  if ((x == LockType::EX) || (y == LockType::EX))
    return true;
  else
    return false;
}

void TxExecutor::letsWound(vector<int> &list, LockType lock_type, Tuple *tuple) {
  int t;
  bool has_conflicts;
  LockType type;
  for (auto it = list.begin(); it != list.end();) {
    t = (*it);
    type = (LockType)tuple->req_type[t];
    has_conflicts = false;
    if (thid_ != t && conflict(lock_type, type)) {
      has_conflicts = true;
    }
    if (has_conflicts == true && thread_timestamp[thid_] <= thread_timestamp[t]) {
      thread_stats[t] = 1;
      it = byeWound(t, tuple);
    }
    else {
      ++it;
    }
  }
}

void TxExecutor::LockWr(LockType EX_lock, Tuple *tuple) {
  tuple->req_type[thid_] = EX_lock;
  while (1) {
    if (tuple->lock_.w_trylock()) {
      letsWound(tuple->currentwriters, EX_lock, tuple);
      tuple->sortAdd(thid_, tuple->waiterlist);
      int t;
      int currentwriter;
      LockType t_type;
      LockType currentwriters_type;
      bool currentwriter_exists = false;
      while (tuple->waiterlist.size()) {
        if (tuple->currentwriters.size()) {
          currentwriter = tuple->currentwriters[0];
          currentwriters_type = (LockType)tuple->req_type[currentwriter];
          currentwriter_exists = true;
        }
        t = tuple->waiterlist[0];
        t_type = (LockType)tuple->req_type[t];
        if (currentwriter_exists && conflict(t_type, currentwriters_type)) {
          break;
        }
        tuple->remove(t, tuple->waiterlist);
        tuple->currentwritersAdd(t);
      }
      tuple->lock_.w_unlock();
      return;
    }
    usleep(1);
  }
}

vector<int>::iterator TxExecutor::byeWound(int txn, Tuple *tuple) {
  bool was_head = false;
  LockType type = (LockType)tuple->req_type[txn];
  int head;
  LockType head_type;
  auto it = tuple->release(txn);
  tuple->req_type[txn] = 0;
  return it;
}

bool TxExecutor::Unlock(uint64_t key, Tuple *tuple) {
  bool was_head;
  LockType type = (LockType)tuple->req_type[thid_];
  int head;
  LockType head_type;
  while (1) {
    if (tuple->lock_.w_trylock()) {
      if (tuple->req_type[thid_] == 0) {
        tuple->lock_.w_unlock();
        return false;
      }
      if (tuple->currentwritersRemove(thid_) == false) {
        exit(1);
      }
      tuple->req_type[thid_] = 0;
      int t;
      int currentwriter;
      LockType t_type;
      LockType currentwriters_type;
      bool currentwriter_exists = false;
      while (tuple->waiterlist.size()) {
        if (tuple->currentwriters.size()) {
          currentwriter = tuple->currentwriters[0];
          currentwriters_type = (LockType)tuple->req_type[currentwriter];
          currentwriter_exists = true;
        }
        t = tuple->waiterlist[0];
        t_type = (LockType)tuple->req_type[t];
        if (currentwriter_exists && conflict(t_type, currentwriters_type)) {
          break;
        }
        tuple->remove(t, tuple->waiterlist);
        tuple->currentwritersAdd(t);
      }
      tuple->lock_.w_unlock();
      return true;
    }
    if (tuple->req_type[thid_] == 0)
      return false;
    usleep(1);
  }
}

vector<int>::iterator Tuple::release(int txn) {
  vector<int>::iterator it;
  int i;
  for (i = 0; i < currentwriters.size(); i++) {
    if (txn == currentwriters[i]) {
      it = currentwriters.erase(currentwriters.begin() + i);
      return it;
    }
  }
  exit(1);
}

bool Tuple::currentwritersRemove(int txn) {
  for (int i = 0; i < currentwriters.size(); i++) {
    if (txn == currentwriters[i]) {
      currentwriters.erase(currentwriters.begin() + i);
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

bool TxExecutor::letsWait(uint64_t key, Tuple *tuple) {
  while (1) {
    if (tuple->lock_.w_trylock()) {
      for (int i = 0; i < tuple->currentwriters.size(); i++) {
        if (thid_ == tuple->currentwriters[i]) {
          if (tuple->req_type[thid_] == -1) {
            read_set_.emplace_back(key, tuple, tuple->val_);
          }
          else if (tuple->req_type[thid_] == 1) {
            write_set_.emplace_back(key, tuple, tuple->val_);
          }
          tuple->lock_.w_unlock();
          return true;
        }
      }
      if (thread_stats[thid_] == 1) {
        tuple->req_type[thid_] = 0;
        if (tuple->remove(thid_, tuple->waiterlist)) {
          int t;
          int currentwriter;
          LockType t_type;
          LockType currentwriters_type;
          bool currentwriter_exists = false;
          while (tuple->waiterlist.size()) {
            if (tuple->currentwriters.size()) {
              currentwriter = tuple->currentwriters[0];
              currentwriters_type = (LockType)tuple->req_type[currentwriter];
              currentwriter_exists = true;
            }
            t = tuple->waiterlist[0];
            t_type = (LockType)tuple->req_type[t];
            if (currentwriter_exists && conflict(t_type, currentwriters_type)) {
              break;
            }
            tuple->remove(t, tuple->waiterlist);
            tuple->currentwritersAdd(t);
          }
          tuple->lock_.w_unlock();
          return false;
        }
        tuple->currentwritersRemove(thid_);
        int t;
        int currentwriter;
        LockType t_type;
        LockType currentwriters_type;
        bool currentwriter_exists = false;
        while (tuple->waiterlist.size()) {
          if (tuple->currentwriters.size()) {
            currentwriter = tuple->currentwriters[0];
            currentwriters_type = (LockType)tuple->req_type[currentwriter];
            currentwriter_exists = true;
          }
          t = tuple->waiterlist[0];
          t_type = (LockType)tuple->req_type[t];
          if (currentwriter_exists && conflict(t_type, currentwriters_type)) {
            break;
          }
          tuple->remove(t, tuple->waiterlist);
          tuple->currentwritersAdd(t);
        }
        tuple->lock_.w_unlock();
        return false;
      }
      tuple->lock_.w_unlock();
    }
    usleep(1);
  }
}

bool TxExecutor::LockUpgr(uint64_t key, Tuple *tuple) {
  while (1) {
    if (tuple->lock_.w_trylock()) {
      letsWound(tuple->currentwriters, LockType::EX, tuple);
        if (tuple->currentwriters.size() == 1 && tuple->currentwriters[0] == thid_) {
          tuple->req_type[thid_] = LockType::EX;
          tuple->lock_.w_unlock();
          return true;
        }
      if (thread_stats[thid_] == 1) {
        tuple->lock_.w_unlock();
        return false;
      }
      tuple->lock_.w_unlock();
    }
    usleep(1);
  }
}

bool TxExecutor::LockRd(LockType SH_lock, Tuple *tuple) {
  tuple->req_type[thid_] = SH_lock;
  bool is_abort = false;
  while (1) {
    if (tuple->lock_.w_trylock()) {
      while (tuple->committing) {
        letsWound(tuple->currentwriters, SH_lock, tuple);
      }
      tuple->sortAdd(thid_, tuple->waiterlist);
      int t;
      int currentwriter;
      LockType t_type;
      LockType currentwriters_type;
      bool currentwriter_exists = false;
      while (tuple->waiterlist.size()) {
        if (tuple->currentwriters.size()) {
          currentwriter = tuple->currentwriters[0];
          currentwriters_type = (LockType)tuple->req_type[currentwriter];
          currentwriter_exists = true;
        }
        t = tuple->waiterlist[0];
        t_type = (LockType)tuple->req_type[t];
        if (currentwriter_exists && conflict(t_type, currentwriters_type)) {
          break;
        }
        tuple->remove(t, tuple->waiterlist);
        tuple->currentwritersAdd(t);
      }
      tuple->lock_.w_unlock();
      return is_abort;
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