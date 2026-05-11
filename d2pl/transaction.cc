
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

SetElement<Tuple> *TxExecutor::searchReadSet(Storage s, std::string_view key) {
  for (auto &re : read_set_) {
    if (re.storage_ != s) continue;
    if (re.key_ == key) return &re;
  }

  return nullptr;
}

SetElement<Tuple> *TxExecutor::searchWriteSet(Storage s, std::string_view key) {
  for (auto &we : write_set_) {
    if (we.storage_ != s) continue;
    if (we.key_ == key) return &we;
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

  ++result_->local_abort_counts_;

#if BACK_OFF
#if ADD_ANALYSIS
  uint64_t start(rdtscp());
#endif

  Backoff::backoff(FLAGS_clocks_per_us);

#if ADD_ANALYSIS
  result_->local_backoff_latency_ += rdtscp() - start;
#endif

#endif
}

/**
 * @brief success termination of transaction.
 * @return void
 */
bool TxExecutor::commit() {
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    switch ((*itr).op_) {
      case OpType::UPDATE: {
        memcpy((*itr).rcdptr_->body_.get_val_ptr(),
               (*itr).body_.get_val_ptr(), (*itr).body_.get_val_size());
        break;
      }
      case OpType::INSERT: {
        break;
      }
      case OpType::DELETE: {
        Status stat = Masstrees[get_storage((*itr).storage_)].remove_value((*itr).key_);       
        // create information for garbage collection
        gc_records_.push_back((*itr).rcdptr_);
        break;
      }
      default:
        ERR;
    }
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

  return true;
}

/**
 * @brief Initialize function of transaction.
 * Allocate timestamp.
 * @return void
 */
void TxExecutor::begin() { this->status_ = TransactionStatus::inflight; }

/**
 * @brief Transaction read function.
 * @param [in] key The key of key-value
 */
Status TxExecutor::read(Storage s, std::string_view key, TupleBody** body) {
#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif  // ADD_ANALYSIS
  TupleBody b;
  SetElement<Tuple>* e;

  /**
   * read-own-writes or re-read from local read set.
   */
  e = searchReadSet(s, key);
  if (e) {
    *body = &(e->body_);
    goto FINISH_READ;
  }
  e = searchWriteSet(s, key);
  if (e) {
    *body = &(e->body_);
    goto FINISH_READ;
  }

  /**
   * Search tuple from data structure.
   */
  Tuple *tuple;
  tuple = Masstrees[get_storage(s)].get_value(key);
#if ADD_ANALYSIS
  ++result_->local_tree_traversal_;
#endif
  if (tuple == nullptr) return Status::WARN_NOT_FOUND;

  read_internal(s, key, tuple);
  *body = &(read_set_.back().body_);

FINISH_READ:

#if ADD_ANALYSIS
  result_->local_read_latency_ += rdtscp() - start;
#endif
  return Status::OK;
}

void TxExecutor::read_internal(Storage s, std::string_view key, Tuple* tuple) {
  TupleBody body;

  /**
   * read payload.
   */
  body = TupleBody(tuple->body_.get_key(), tuple->body_.get_val(), tuple->body_.get_val_align());
  read_set_.emplace_back(s, key, tuple, std::move(body));

FINISH_READ:
  return;
}

Status TxExecutor::scan(const Storage s,
                      std::string_view left_key, bool l_exclusive,
                      std::string_view right_key, bool r_exclusive,
                      std::vector<TupleBody*>& result) {
  result.clear();
  auto rset_init_size = read_set_.size();

  std::vector<Tuple*> scan_res;
  Masstrees[get_storage(s)].scan(
            left_key.empty() ? nullptr : left_key.data(), left_key.size(),
            l_exclusive, right_key.empty() ? nullptr : right_key.data(),
            right_key.size(), r_exclusive, &scan_res, false);

  for (auto &&itr : scan_res) {
    SetElement<Tuple>* e = searchReadSet(s, itr->body_.get_key());
    if (e) {
      result.emplace_back(&(e->body_));
      continue;
    }

    e = searchWriteSet(s, itr->body_.get_key());
    if (e) {
      result.emplace_back(&(e->body_));
      continue;
    }

    read_internal(s, itr->body_.get_key(), itr);
  }

  if (rset_init_size != read_set_.size()) {
    for (auto itr = read_set_.begin() + rset_init_size;
         itr != read_set_.end(); ++itr) {
      result.emplace_back(&((*itr).body_));
    }
  }

  return Status::OK;
}

/**
 * @brief transaction write operation
 * @param [in] key The key of key-value
 * @return void
 */
Status TxExecutor::write(Storage s, std::string_view key, TupleBody&& body) {
#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif

  // if it already wrote the key object once.
  if (searchWriteSet(s, key)) goto FINISH_WRITE;

  /**
   * Search tuple from data structure.
   */
  Tuple *tuple;
  tuple = Masstrees[get_storage(s)].get_value(key);
#if ADD_ANALYSIS
    ++result_->local_tree_traversal_;
#endif
  if (tuple == nullptr) return Status::WARN_NOT_FOUND;

  write_set_.emplace_back(s, key, tuple, std::move(body), OpType::UPDATE);

FINISH_WRITE:
#if ADD_ANALYSIS
  result_->local_write_latency_ += rdtscp() - start;
#endif  // ADD_ANALYSIS
  return Status::OK;
}

Status TxExecutor::insert(Storage s, std::string_view key, TupleBody&& body) {
#if ADD_ANALYSIS
  std::uint64_t start = rdtscp();
#endif

  if (searchWriteSet(s, key)) return Status::WARN_ALREADY_EXISTS;

  Tuple* tuple = Masstrees[get_storage(s)].get_value(key);
#if ADD_ANALYSIS
  ++result_->local_tree_traversal_;
#endif
  if (tuple != nullptr) {
    return Status::WARN_ALREADY_EXISTS;
  }

  tuple = new Tuple();
  tuple->init(std::move(body));

  Status stat = Masstrees[get_storage(s)].insert_value(key, tuple);
  if (stat == Status::WARN_ALREADY_EXISTS) {
    delete tuple;
    return stat;
  }

  write_set_.emplace_back(s, key, tuple, OpType::INSERT);
  w_lock_list_.emplace_back(&tuple->lock_);

#if ADD_ANALYSIS
  result_->local_write_latency_ += rdtscp() - start;
#endif
  return Status::OK;
}

Status TxExecutor::delete_record(Storage s, std::string_view key) {
#if ADD_ANALYSIS
  std::uint64_t start = rdtscp();
#endif

  // cancel previous write
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    if ((*itr).storage_ != s) continue;
    if ((*itr).key_ == key) {
      write_set_.erase(itr);
    }
  }

  Tuple* tuple = Masstrees[get_storage(s)].get_value(key);
#if ADD_ANALYSIS
  ++result_->local_tree_traversal_;
#endif
  if (tuple == nullptr) {
    return Status::WARN_NOT_FOUND;
  }

  write_set_.emplace_back(s, key, tuple, OpType::DELETE);

#if ADD_ANALYSIS
  result_->local_write_latency_ += rdtscp() - start;
#endif
  return Status::OK;
}

/**
 * @brief lock all target records.
 * @return void
 */
bool TxExecutor::lockList() {
  std::sort(lock_entries_.begin(), lock_entries_.end());
  for (auto& le : lock_entries_) {
    Tuple *tuple;
    tuple = Masstrees[get_storage(le.storage_)].get_value(le.key_);
    if (tuple == nullptr) {
      std::stringstream ss;
      ss << "WARN: key not found " << (uint32_t)le.storage_ << " " << str_view_hex(le.key_);
      dump(thid_, ss.str());
      return false;
    }
    if (le.is_exclusive_) {
      //if (thid_ == 1) std::cout << "WL: " << tuple << std::endl;
      tuple->lock_.w_lock();
      w_lock_list_.emplace_back(&tuple->lock_);
    } else {
      //if (thid_ == 0) std::cout << "RL: " << tuple << std::endl;
      tuple->lock_.r_lock();
      r_lock_list_.emplace_back(&tuple->lock_);
    }
  }
  return true;
}

/**
 * @brief unlock and clean-up local lock set.
 * @return void
 */
void TxExecutor::unlockList() {
  for (auto itr = r_lock_list_.begin(); itr != r_lock_list_.end(); ++itr)
    (*itr)->r_unlock();

  for (auto itr = w_lock_list_.begin(); itr != w_lock_list_.end(); ++itr) {
    // if (thid_ == 2) {
      // std::stringstream ss; ss << "unlock: " << (*itr);
      // dump(thid_, ss.str());
    // }
    (*itr)->w_unlock();
  }

  /**
   * Clean-up local lock set.
   */
  r_lock_list_.clear();
  w_lock_list_.clear();

  lock_entries_.clear();
}

void TxExecutor::reconnoiter_begin() {
    reconnoitering_ = true;
}

void TxExecutor::reconnoiter_end() {
    read_set_.clear();
    reconnoitering_ = false;
    begin();
}

bool TxExecutor::isLeader() {
  return this->thid_ == 0;
}

void TxExecutor::leaderWork() {
#if BACK_OFF
  leaderBackoffWork(backoff_, D2PLResult);
#endif
}
