#include <stdio.h>

#include <algorithm>
#include <fstream>
#include <string>

#include "../include/atomic_wrapper.hh"
#include "../include/backoff.hh"
#include "../include/masstree_wrapper.hh"
#include "include/atomic_tool.hh"
#include "include/common.hh"
#include "include/scan_callback.hh"
#include "include/transaction.hh"
#include "include/tuple.hh"

using namespace std;

extern std::vector<Result> TicTocResult;
extern void tictocLeaderWork(uint64_t &epoch_timer_start, uint64_t &epoch_timer_stop);

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

void TxExecutor::begin() {
  this->status_ = TransactionStatus::inflight;
  this->commit_ts_ = 0;
  this->appro_commit_ts_ = 0;
  atomicStoreThLocalEpoch(thid_, atomicLoadGE());
}

bool TxExecutor::preemptiveAborts(const TsWord &v1) {
  if (v1.rts() < this->appro_commit_ts_) {
    /**
     * it must check whether this write set include the tuple,
     * but it already checked.
     * so it must abort.
     */
    this->status_ = TransactionStatus::aborted;
#if ADD_ANALYSIS
    ++result_->local_preemptive_aborts_counts_;
#endif
    return true;
  }

  return false;
}

Status TxExecutor::read(Storage s, std::string_view key, TupleBody** body) {
#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif

#if SLEEP_READ_PHASE
  sleepTics(SLEEP_READ_PHASE);
#endif

  /**
   * these variable cause error (-fpermissive)
   * "crosses initialization of ..."
   * So it locate before first goto instruction.
   */
  TsWord v1, v2;
  TupleBody b;
  SetElement<Tuple>* e;
  Status stat;

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

  stat = read_internal(s, key, tuple);
  if (stat != Status::OK) {
    return stat;
  }
  *body = &(read_set_.back().body_);

FINISH_READ:

#if ADD_ANALYSIS
  result_->local_read_latency_ += rdtscp() - start;
#endif
  return Status::OK;
}

Status TxExecutor::read_internal(Storage s, std::string_view key, Tuple* tuple) {
  /**
   * these variable cause error (-fpermissive)
   * "crosses initialization of ..."
   * So it locate before first goto instruction.
   */
  TsWord v1, v2;
  TupleBody b;

  v1.obj_ = __atomic_load_n(&(tuple->tsw_.obj_), __ATOMIC_ACQUIRE);
  for (;;) {
    if (v1.lock) {
      /**
       * Check whether it can be serialized between old points and new points.
       */
#if PREEMPTIVE_ABORTS
      if (preemptiveAborts(v1)) return Status::ERROR_PREEMPTIVE_ABORT;
#endif
      v1.obj_ = __atomic_load_n(&(tuple->tsw_.obj_), __ATOMIC_ACQUIRE);
      continue;
    }

    if (v1.absent) {
      return Status::WARN_NOT_FOUND;
    }

    /**
     * read payload.
     */
    b = TupleBody(tuple->body_.get_key(), tuple->body_.get_val(), tuple->body_.get_val_align());

    v2.obj_ = __atomic_load_n(&(tuple->tsw_.obj_), __ATOMIC_ACQUIRE);
    if (v1 == v2 && !v1.lock) break;
    /**
     * else: update by new tsw_ and retry with using this one.
     */
    v1 = v2;
#if ADD_ANALYSIS
    ++result_->local_extra_reads_;
#endif
  }

  this->appro_commit_ts_ = max(this->appro_commit_ts_, v1.wts);
  read_set_.emplace_back(s, key, tuple, std::move(b), v1);

FINISH_READ:
  return Status::OK;
}

Status TxExecutor::scan(const Storage s,
                        std::string_view left_key, bool l_exclusive,
                        std::string_view right_key, bool r_exclusive,
                        std::vector<TupleBody*>& result) {
  return scan(s, left_key, l_exclusive, right_key, r_exclusive, result, -1);
}

Status TxExecutor::scan(const Storage s,
                        std::string_view left_key, bool l_exclusive,
                        std::string_view right_key, bool r_exclusive,
                        std::vector<TupleBody*>& result, int64_t limit) {
  result.clear();
  auto rset_init_size = read_set_.size();

  std::vector<Tuple*> scan_res;
  Masstrees[get_storage(s)].scan(
            left_key.empty() ? nullptr : left_key.data(), left_key.size(),
            l_exclusive, right_key.empty() ? nullptr : right_key.data(),
            right_key.size(), r_exclusive, &scan_res, limit,
            callback_);

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

    Status stat = read_internal(s, itr->body_.get_key(), itr);
    if (stat != Status::OK && stat != Status::WARN_NOT_FOUND) {
      return stat;
    }
  }

  if (rset_init_size != read_set_.size()) {
    for (auto itr = read_set_.begin() + rset_init_size;
         itr != read_set_.end(); ++itr) {
      result.emplace_back(&((*itr).body_));
    }
  }

  return Status::OK;
}

Status TxExecutor::write(Storage s, std::string_view key, TupleBody&& body) {
#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif

  // these variable cause error (-fpermissive)
  // "crosses initialization of ..."
  // So it locate before first goto instruction.
  TsWord tsword;

  if (searchWriteSet(s, key)) goto FINISH_WRITE;

  /**
   * Search tuple from data structure.
   */
  Tuple *tuple;
  SetElement<Tuple> *re;
  re = searchReadSet(s, key);
  if (re) {
    tuple = re->rcdptr_;
  } else {
    tuple = Masstrees[get_storage(s)].get_value(key);
#if ADD_ANALYSIS
    ++result_->local_tree_traversal_;
#endif
    if (tuple == nullptr) return Status::WARN_NOT_FOUND;
  }

  tsword.obj_ = __atomic_load_n(&(tuple->tsw_.obj_), __ATOMIC_ACQUIRE);
  this->appro_commit_ts_ = max(this->appro_commit_ts_, tsword.rts() + 1);
  write_set_.emplace_back(s, key, tuple, std::move(body), tsword, OpType::UPDATE);

FINISH_WRITE:;
#if ADD_ANALYSIS
  result_->local_write_latency_ += rdtscp() - start;
#endif
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

  typename MasstreeWrapper<Tuple>::insert_info_t insert_info;
  Status stat = Masstrees[get_storage(s)].insert_value(key, tuple, &insert_info);
  if (stat == Status::WARN_ALREADY_EXISTS) {
    delete tuple;
    return stat;
  }
  if (insert_info.node) {
    if (!node_map_.empty()) {
      auto it = node_map_.find((void*)insert_info.node);
      if (it != node_map_.end()) {
        if (unlikely(it->second != insert_info.old_version)) {
          status_ = TransactionStatus::aborted;
          return Status::ERROR_CONCURRENT_WRITE_OR_DELETE;
        }
        // otherwise, bump the version
        it->second = insert_info.new_version;
      }
    }
  } else {
    ERR;
  }

  write_set_.emplace_back(s, key, tuple, tuple->tsw_, OpType::INSERT);

#if ADD_ANALYSIS
  result_->local_write_latency_ += rdtscp() - start;
#endif
  return Status::OK;
}

Status TxExecutor::delete_record(Storage s, std::string_view key) {
#if ADD_ANALYSIS
  std::uint64_t start = rdtscp();
#endif
  TsWord tsword;

  // cancel previous write
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    if ((*itr).storage_ != s) continue;
    if ((*itr).key_ == key) {
      write_set_.erase(itr);
    }
  }

  Tuple *tuple;
  SetElement<Tuple> *re;
  re = searchReadSet(s, key);
  if (re) {
    tuple = re->rcdptr_;
  } else {
    tuple = Masstrees[get_storage(s)].get_value(key);
#if ADD_ANALYSIS
    ++result_->local_tree_traversal_;
#endif
    if (tuple == nullptr) return Status::WARN_NOT_FOUND;
  }

  tsword.obj_ = __atomic_load_n(&(tuple->tsw_.obj_), __ATOMIC_ACQUIRE);
  this->appro_commit_ts_ = max(this->appro_commit_ts_, tsword.rts() + 1);
  write_set_.emplace_back(s, key, tuple, tsword, OpType::DELETE);

#if ADD_ANALYSIS
  result_->local_write_latency_ += rdtscp() - start;
#endif
  return Status::OK;
}

bool TxExecutor::validationPhase() {
#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif

  lockWriteSet();
#if NO_WAIT_LOCKING_IN_VALIDATION
  if (this->status_ == TransactionStatus::aborted) {
#if ADD_ANALYSIS
    result_->local_vali_latency_ += rdtscp() - start;
#endif
    return false;
  }
#endif

  /**
   * logically, it must execute full fence here,
   * while we assume intel architecture and CAS(cmpxchg) in lockWriteSet() did
   * it.
   */

  asm volatile("":: : "memory");

  // step2, compute the commit timestamp
  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
    /**
     * Originally, commit_ts is calculated by two loops
     * (read set loop, write set loop).
     * However, it is bad for performances.
     * Write set loop should be merged with write set (lock) loop.
     * Read set loop should be merged with read set (validation) loop.
     * The result reduces two loops and improves performance.
     */
    commit_ts_ = max(commit_ts_, (*itr).tsw_.wts);
  }
  // step3, validate the read set.
  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
    TsWord v1, v2;
#if ADD_ANALYSIS
    ++result_->local_rtsupd_chances_;
#endif

    v1.obj_ = __atomic_load_n(&((*itr).rcdptr_->tsw_.obj_), __ATOMIC_ACQUIRE);
    if ((*itr).tsw_.rts() < commit_ts_) {
      for (;;) {
        if ((*itr).tsw_.wts != v1.wts) {
          // start timestamp history processing
#if TIMESTAMP_HISTORY
          TsWord pre_v1;
          pre_v1.obj_ = __atomic_load_n(&((*itr).rcdptr_->pre_tsw_.obj_),
                                        __ATOMIC_ACQUIRE);
          if (pre_v1.wts <= commit_ts_ && commit_ts_ < v1.wts) {
            /**
             * Success
             */
#if ADD_ANALYSIS
            ++result_->local_timestamp_history_success_counts_;
#endif
            break;
          }
          /**
           * Fail
           */
#if ADD_ANALYSIS
          ++result_->local_timestamp_history_fail_counts_;
#endif
#endif
          // end timestamp history processing
#if ADD_ANALYSIS
          result_->local_vali_latency_ += rdtscp() - start;
#endif
          unlockWriteSet();
          return false;
        }

        SetElement<Tuple> *inW = searchWriteSet((*itr).storage_, (*itr).key_);
        if ((v1.rts()) < commit_ts_ && v1.lock) {
          if (inW == nullptr) {
            /**
             * It needs to update read timestamp.
             * However, v1 is locked and this transaction didn't lock it,
             * so other transaction locked.
             */
#if ADD_ANALYSIS
            result_->local_vali_latency_ += rdtscp() - start;
#endif
            unlockWriteSet();
            return false;
          }
        }

        /**
         * If this transaction already did write operation,
         * it can update timestamp at write phase.
         */
        if (inW != nullptr) break;
        // extend the rts of the tuple
        if ((v1.rts()) < commit_ts_) {
          // Handle delta overflow
          uint64_t delta = commit_ts_ - v1.wts;
          uint64_t shift = delta - (delta & 0x7fff);
          v2.obj_ = v1.obj_;
          v2.wts = v2.wts + shift;
          v2.delta = delta - shift;
          if (__atomic_compare_exchange_n(&((*itr).rcdptr_->tsw_.obj_),
                                          &(v1.obj_), v2.obj_, false,
                                          __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
#if ADD_ANALYSIS
            ++result_->local_rtsupd_;
#endif
            break;
          } else
            continue;
        } else {
          break;
        }
      }
    }
  }
  // step 4, validate the node set
  for (auto it : node_map_) {
    auto node = (MasstreeWrapper<Tuple>::node_type *) it.first;
    if (node->full_version_value() != it.second) {
      this->status_ = TransactionStatus::aborted;
      unlockWriteSet();
      return false;
    }
  }
#if ADD_ANALYSIS
  result_->local_vali_latency_ += rdtscp() - start;
#endif
  return true;
}

void TxExecutor::abort() {
  // remove inserted records
  for (auto& we : write_set_) {
    if (we.op_ == OpType::INSERT) {
      Masstrees[get_storage(we.storage_)].remove_value(we.key_);
      delete we.rcdptr_;
    }
  }

  gc_records();

  /**
   * Clean-up local read/write set.
   */
  read_set_.clear();
  write_set_.clear();
  node_map_.clear();

#if BACK_OFF

#if ADD_ANALYSIS
  uint64_t start(rdtscp());
#endif
  Backoff::backoff(FLAGS_clocks_per_us);
#if ADD_ANALYSIS
  ++result_->local_backoff_latency_ += rdtscp() - start;
#endif

#endif
}

void TxExecutor::writePhase() {
  TsWord result;

  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    switch ((*itr).op_) {
      case OpType::UPDATE: {
        result.absent = false;
        memcpy((*itr).rcdptr_->body_.get_val_ptr(),
              (*itr).body_.get_val_ptr(), (*itr).body_.get_val_size());
#if TIMESTAMP_HISTORY
        __atomic_store_n(&((*itr).rcdptr_->pre_tsw_.obj_), (*itr).tsw_.obj_, __ATOMIC_RELAXED);
#endif
        break;
      }
      case OpType::INSERT: {
        result.absent = false;
        break;
      }
      case OpType::DELETE: {
        result.absent = true;
        Status stat = Masstrees[get_storage((*itr).storage_)].remove_value((*itr).key_);
        gc_records_.emplace_back((*itr).storage_, (*itr).key_, (*itr).rcdptr_, ThLocalEpoch[thid_].obj_);
        break;
      }
      default:
        ERR;
    }
    result.wts = this->commit_ts_;
    result.delta = 0;
    result.lock = 0;
    __atomic_store_n(&((*itr).rcdptr_->tsw_.obj_), result.obj_, __ATOMIC_RELEASE);
  }

  gc_records();

  /**
   * Clean-up local read/write/lock set.
   */
  read_set_.clear();
  write_set_.clear();
  node_map_.clear();
}

void TxExecutor::lockWriteSet() {
  TsWord expected, desired;

  /**
   * For dead lock avoidance.
   */
  sort(write_set_.begin(), write_set_.end());

[[maybe_unused]] retry
  :
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    if (itr->op_ == OpType::INSERT) continue;
    expected.obj_ =
            __atomic_load_n(&((*itr).rcdptr_->tsw_.obj_), __ATOMIC_ACQUIRE);
    for (;;) {
      if (expected.lock) {
        if (this->is_wonly_ == false) {
#if NO_WAIT_LOCKING_IN_VALIDATION
          /**
           * no-wait locking in validation
           */
          this->status_ = TransactionStatus::aborted;
          /**
           * unlock locked record.
           */
          if (itr != write_set_.begin()) {
            unlockWriteSet(itr);
          }
          return;
#elif NO_WAIT_OF_TICTOC
          if (itr != write_set_.begin()) unlockWriteSet(itr);
          /**
           * pre-verify
           */
          for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
            TsWord v1, v2;

            v1.obj_ =
                __atomic_load_n(&((*itr).rcdptr_->tsw_.obj_), __ATOMIC_ACQUIRE);
            if ((*itr).tsw_.rts() < commit_ts_) {
              if ((*itr).tsw_.wts != v1.wts) {
                // start timestamp history processing
#if TIMESTAMP_HISTORY
                TsWord pre_v1;
                pre_v1.obj_ = __atomic_load_n(&((*itr).rcdptr_->pre_tsw_.obj_),
                                              __ATOMIC_ACQUIRE);
                if (pre_v1.wts <= commit_ts_ && commit_ts_ < v1.wts) {
                  /**
                   * Success
                   */
#if ADD_ANALYSIS
                  ++result_->local_timestamp_history_success_counts_;
#endif
                  continue;
                }
                /**
                 * Fail
                 */
#if ADD_ANALYSIS
                ++result_->local_timestamp_history_fail_counts_;
#endif
#endif
                // end timestamp history processing
                this->status_ = TransactionStatus::aborted;
                return;
              }

              if ((v1.rts()) < commit_ts_ && v1.lock) {
                this->status_ = TransactionStatus::aborted;
                return;
              }
            }
          }
          /**
           * end pre-verify
           */
          sleepTics(FLAGS_clocks_per_us);  // sleep 1us.
          if (thid_ == 0) std::cout << "lock retry" << std::endl;
          goto retry;
#endif
        }
        expected.obj_ = loadAcquire((*itr).rcdptr_->tsw_.obj_);
      } else {
        desired = expected;
        desired.lock = 1;
        if (__atomic_compare_exchange_n(&((*itr).rcdptr_->tsw_.obj_),
                                        &(expected.obj_), desired.obj_, false,
                                        __ATOMIC_RELAXED, __ATOMIC_RELAXED))
          break;
      }
    }

    /**
     * Originally, commit_ts is calculated by two loops
     * (read set loop, write set loop).
     * However, it is bad for performances.
     * Write set loop should be merged with write set (lock) loop.
     * Read set loop should be merged with read set (validation) loop.
     * The result reduces two loops and improves performance.
     */
    commit_ts_ = max(commit_ts_, desired.rts() + 1);
  }
}

void TxExecutor::unlockWriteSet() {
  TsWord expected, desired;

  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    if ((*itr).op_ == OpType::INSERT) continue;
    expected.obj_ = loadAcquire((*itr).rcdptr_->tsw_.obj_);
    desired.obj_ = expected.obj_;
    desired.lock = 0;
    storeRelease((*itr).rcdptr_->tsw_.obj_, desired.obj_);
  }
}

void TxExecutor::unlockWriteSet(std::vector<SetElement<Tuple>>::iterator end) {
  TsWord expected, desired;

  for (auto itr = write_set_.begin(); itr != end; ++itr) {
    if ((*itr).op_ == OpType::INSERT) continue;
    expected.obj_ = loadAcquire((*itr).rcdptr_->tsw_.obj_);
    desired.obj_ = expected.obj_;
    desired.lock = 0;
    storeRelease((*itr).rcdptr_->tsw_.obj_, desired.obj_);
  }
}

// TODO: enable this if we want to use
// void TxExecutor::dispWS() {
//   cout << "th " << this->thid_ << ": write set: ";

//   for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
//     cout << "(" << (*itr).key_ << ", " << (*itr).val_ << "), ";
//   }
//   cout << endl;
// }

bool TxExecutor::commit() {
  if (validationPhase()) {
    writePhase();
    return true;
  } else {
    return false;
  }
}

void TxExecutor::gc_records() {
  const auto r_epoch = ReclamationEpoch;

  // for records
  while (!gc_records_.empty()) {
    auto gce = gc_records_.front();
    if (gce.epoch_ > r_epoch) break;
    delete gce.rcdptr_;
    gc_records_.pop_front();
  }
}

bool TxExecutor::isLeader() {
  return this->thid_ == 0;
}

void TxExecutor::leaderWork() {
  tictocLeaderWork(this->epoch_timer_start, this->epoch_timer_stop);
#if BACK_OFF
  leaderBackoffWork(backoff_, TicTocResult);
#endif
}

void TxExecutor::reconnoiter_begin() {
  reconnoitering_ = true;
}

void TxExecutor::reconnoiter_end() {
  read_set_.clear();
  node_map_.clear();
  reconnoitering_ = false;
  begin();
}

void TxScanCallback::on_resp_node(const MasstreeWrapper<Tuple>::node_type *n, uint64_t version) {
  auto it = tx_->node_map_.find((void*)n);
  if (it == tx_->node_map_.end()) {
    tx_->node_map_.emplace_hint(it, (void*)n, version);
  } else if ((*it).second != version) {
    tx_->status_ = TransactionStatus::aborted;
  }
}
