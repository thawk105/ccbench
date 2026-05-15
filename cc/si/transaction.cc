#include <algorithm>
#include <atomic>
#include <bitset>
#include <set>

#include "../../include/atomic_wrapper.hh"
#include "../../include/backoff.hh"
#include "../../include/masstree_wrapper.hh"
#include "include/common.hh"
#include "include/scan_callback.hh"
#include "include/transaction.hh"
#include "include/version.hh"

extern bool chkClkSpan(const uint64_t start, const uint64_t stop,
                       const uint64_t threshold);

using namespace std;

/**
 * @brief Initialize function of transaction.
 * Allocate timestamp.
 * @return void
 */
void TxExecutor::begin() {
  TransactionTable *newElement, *tmt;

  tmt = loadAcquire(TMT[thid_]);
  uint32_t lastcstamp;
  if (this->status_ == TransactionStatus::aborted) {
    /**
     * If this transaction is retry by abort,
     * its lastcstamp is last one.
     */
    lastcstamp = this->txid_ = tmt->lastcstamp_.load(memory_order_acquire);
  } else {
    /**
     * If this transaction is after committed transaction,
     * its lastcstamp is that's one.
     */
    lastcstamp = this->txid_ = cstamp_;
  }

  if (gcobject_.reuse_TMT_element_from_gc_.empty()) {
    /**
     * If no cache,
     */
    newElement = new TransactionTable(0, 0, UINT32_MAX, lastcstamp,
                                      TransactionStatus::inflight);
#if ADD_ANALYSIS
    ++result_->local_TMT_element_malloc_;
#endif
  } else {
    /**
     * If it has cache, this transaction use it.
     */
    newElement = gcobject_.reuse_TMT_element_from_gc_.back();
    gcobject_.reuse_TMT_element_from_gc_.pop_back();
    newElement->set(0, 0, UINT32_MAX, lastcstamp, TransactionStatus::inflight);
#if ADD_ANALYSIS
    ++result_->local_TMT_element_reuse_;
#endif
  }

  /**
   * Check the latest commit timestamp
   */
  for (unsigned int i = 0; i < TotalThreadNum; ++i) {
    tmt = loadAcquire(TMT[i]);
    this->txid_ = max(this->txid_, tmt->lastcstamp_.load(memory_order_acquire));
  }
  this->txid_ += 1;
  newElement->txid_ = this->txid_;

  /**
   * Old object becomes cache object.
   */
  gcobject_.gcq_for_TMT_.emplace_back(loadAcquire(TMT[thid_]));
  /**
   * New object is registerd to transaction mapping table.
   */
  storeRelease(TMT[thid_], newElement);

  pstamp_ = 0;
  sstamp_ = UINT32_MAX;
  status_ = TransactionStatus::inflight;
}

/**
 * @brief Transaction read function.
 * @param [in] key The key of key-value
 */
Status TxExecutor::read(Storage s, std::string_view key, TupleBody** body) {
#if ADD_ANALYSIS
  uint64_t start(rdtscp());
#endif

  /**
   * read-own-writes, re-read from previous read in the same tx.
   */
  SetElement<Tuple>* e = searchReadSet(s, key);
  if (e) {
    *body = &(e->ver_->body_);
    goto FINISH_READ;
  }
  e = searchWriteSet(s, key);
  if (e) {
    if (e->op_ == OpType::DELETE) {
      // deleted by myself
      return Status::WARN_NOT_FOUND;
    }
    *body = &(e->ver_->body_);
    goto FINISH_READ;
  }

  /**
   * Search versions from data structure.
   */
  Tuple* tuple;
  tuple = Masstrees[get_storage(s)].get_value(key);
#if ADD_ANALYSIS
  ++result_->local_tree_traversal_;
#endif
  if (tuple == nullptr) return Status::WARN_NOT_FOUND;

  Version* ver;
  ver = read_internal(s, key, tuple);
  if (ver == nullptr ||
      ver->status_.load(memory_order_acquire) == VersionStatus::deleted)
    return Status::WARN_NOT_FOUND;

  /**
   * read payload.
   */
  *body = &(ver->body_);
#if ADD_ANALYSIS
  ++result_->local_memcpys;
#endif

FINISH_READ:
#if ADD_ANALYSIS
  result_->local_read_latency_ += rdtscp() - start;
#endif
  return Status::OK;
}

Version* TxExecutor::read_internal(Storage s, std::string_view key,
                                   Tuple* tuple) {
  /**
   * Move to the points of this view.
   */
  Version* ver;
  ver = tuple->latest_.load(memory_order_acquire);
  while ((ver->status_.load(memory_order_acquire) != VersionStatus::committed &&
          ver->status_.load(memory_order_acquire) != VersionStatus::deleted) ||
         txid_ < ver->cstamp_.load(memory_order_acquire)) {
    ver = ver->prev_;
    if (ver == nullptr) { return nullptr; }
  }

  // SI: just record the version we observed in the snapshot.
  // (ERMIA tracked sstamp via psstamp_ for SSN's anti-dependency check;
  //  pure SI doesn't validate, so we drop that.)
  read_set_.emplace_back(s, key, tuple, ver);
  upReadersBits(ver);

  return ver;
}

Status TxExecutor::install_version(Tuple* tuple, Version* desired) {
  Version* vertmp;
  Version* expected = tuple->latest_.load(memory_order_acquire);
  for (;;) {
    // w-w conflict
    // first updater wins rule
    if (expected->status_.load(memory_order_acquire) ==
        VersionStatus::inflight) {
      if (this->txid_ <= expected->cstamp_.load(memory_order_acquire)) {
        this->status_ = TransactionStatus::aborted;
        TMT[thid_]->status_.store(TransactionStatus::aborted,
                                  memory_order_release);
        gcobject_.reuse_version_from_gc_.emplace_back(desired);
        return Status::ERROR_CONCURRENT_WRITE_OR_DELETE;
      }

      expected = tuple->latest_.load(memory_order_acquire);
      continue;
    }

    // if latest version is not comitted.
    vertmp = expected;
    while (vertmp->status_.load(memory_order_acquire) !=
               VersionStatus::committed &&
           vertmp->status_.load(memory_order_acquire) != VersionStatus::deleted)
      vertmp = vertmp->prev_;

    // vertmp is latest committed version.
    if (txid_ < vertmp->cstamp_.load(memory_order_acquire)) {
      //  write - write conflict, first-updater-wins rule.
      // Writers must abort if they would overwirte a version created after
      // their snapshot.
      this->status_ = TransactionStatus::aborted;
      TMT[thid_]->status_.store(TransactionStatus::aborted,
                                memory_order_release);
      gcobject_.reuse_version_from_gc_.emplace_back(desired);
      return Status::ERROR_CONCURRENT_WRITE_OR_DELETE;
    }

    desired->prev_ = expected;
    if (tuple->latest_.compare_exchange_strong(
            expected, desired, memory_order_acq_rel, memory_order_acquire))
      break;
  }

  return Status::OK;
}


/**
 * @brief Transaction write function.
 * @param [in] key The key of key-value
 */
Status TxExecutor::update(Storage s, std::string_view key, TupleBody&& body) {
#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif
  Status stat = Status::OK;

  /**
   * update local write set.
   */
  if (searchWriteSet(s, key)) goto FINISH_WRITE;

  /**
   * avoid false positive.
   */
  Tuple* tuple;
  tuple = nullptr;
  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
    if ((*itr).storage_ == s && (*itr).key_ == key) {
      downReadersBits((*itr).ver_);
      /**
       * If it can find record in read set, use this for high performance.
       */
      tuple = (*itr).rcdptr_;
      read_set_.erase(itr);
      break;
    }
  }

  /**
   * Search tuple from data structure.
   */
  if (!tuple) {
    tuple = Masstrees[get_storage(s)].get_value(key);
#if ADD_ANALYSIS
    ++result_->local_tree_traversal_;
#endif
  }
  if (tuple == nullptr) return Status::WARN_NOT_FOUND;

  /**
   * If v not in t.writes:
   * first-updater-wins rule
   * Forbid a transaction to update  a record that has a committed head version
   * later than its begin timestamp.
   */
  Version* desired;
  desired = new Version();
  if (gcobject_.reuse_version_from_gc_.empty()) {
    desired = new Version();
#if ADD_ANALYSIS
    ++result_->local_version_malloc_;
#endif
  } else {
    desired = gcobject_.reuse_version_from_gc_.back();
    gcobject_.reuse_version_from_gc_.pop_back();
    desired->init();
#if ADD_ANALYSIS
    ++result_->local_version_reuse_;
#endif
  }
  desired->cstamp_.store(
      this->txid_, memory_order_relaxed); // read operation, write operation,
  // it is also accessed by garbage collection.

  stat = install_version(tuple, desired);
  if (stat != Status::OK) { goto FINISH_WRITE; }

  // SI: no SSN bookkeeping (sstamp/pstamp updates) — just record the write.
  desired->body_ = std::move(body);
  write_set_.emplace_back(s, key, tuple, desired, OpType::UPDATE);

FINISH_WRITE:
#if ADD_ANALYSIS
  result_->local_write_latency_ += rdtscp() - start;
#endif
  return stat;
}

Status TxExecutor::insert(Storage s, std::string_view key, TupleBody&& body) {
#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif // if ADD_ANALYSIS

  if (searchWriteSet(s, key)) return Status::WARN_ALREADY_EXISTS;

  Tuple* tuple = Masstrees[get_storage(s)].get_value(key);
#if ADD_ANALYSIS
  ++result_->local_tree_traversal_;
#endif
  if (tuple != nullptr) { return Status::WARN_ALREADY_EXISTS; }

  tuple = new Tuple();
  tuple->init(this->txid_, std::move(body));
  Version* ver = tuple->latest_.load(std::memory_order_acquire);
  typename MasstreeWrapper<Tuple>::insert_info_t insert_info;
  Status stat =
      Masstrees[get_storage(s)].insert_value(key, tuple, &insert_info);
  if (stat == Status::WARN_ALREADY_EXISTS) {
    delete tuple;
    return stat;
  }
  if (insert_info.node) {
    if (!node_map_.empty()) {
      auto it = node_map_.find((void*) insert_info.node);
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

  write_set_.emplace_back(s, key, tuple, ver, OpType::INSERT);

#if ADD_ANALYSIS
  result_->local_write_latency_ += rdtscp() - start;
#endif // if ADD_ANALYSIS
  return Status::OK;
}

Status TxExecutor::delete_record(Storage s, std::string_view key) {
#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif // if ADD_ANALYSIS
  Status stat = Status::OK;

  // cancel previous write
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    if ((*itr).storage_ != s) continue;
    if ((*itr).key_ == key) { write_set_.erase(itr); }
  }

  Tuple* tuple = nullptr;
  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
    if ((*itr).storage_ == s && (*itr).key_ == key) {
      downReadersBits((*itr).ver_);
      tuple = (*itr).rcdptr_;
      read_set_.erase(itr);
      break;
    }
  }

  if (!tuple) {
    tuple = Masstrees[get_storage(s)].get_value(key);
#if ADD_ANALYSIS
    ++result_->local_tree_traversal_;
#endif // if ADD_ANALYSIS
    if (tuple == nullptr) return Status::WARN_NOT_FOUND;
  }

  Version* desired;
  desired = new Version();
  if (gcobject_.reuse_version_from_gc_.empty()) {
    desired = new Version();
#if ADD_ANALYSIS
    ++result_->local_version_malloc_;
#endif
  } else {
    desired = gcobject_.reuse_version_from_gc_.back();
    gcobject_.reuse_version_from_gc_.pop_back();
    desired->init();
#if ADD_ANALYSIS
    ++result_->local_version_reuse_;
#endif
  }
  desired->cstamp_.store(
      this->txid_, memory_order_relaxed); // read operation, write operation,

  // it is also accessed by garbage collection.

  stat = install_version(tuple, desired);
  if (stat != Status::OK) { goto FINISH_DELETE; }

  // SI: no SSN bookkeeping for deletes either.
  write_set_.emplace_back(s, key, tuple, desired, OpType::DELETE);

FINISH_DELETE:
#if ADD_ANALYSIS
  result_->local_write_latency_ += rdtscp() - start;
#endif
  return stat;
}

Status TxExecutor::scan(const Storage s, std::string_view left_key,
                        bool l_exclusive, std::string_view right_key,
                        bool r_exclusive, std::vector<TupleBody*>& result) {
  return scan(s, left_key, l_exclusive, right_key, r_exclusive, result, -1);
}

Status TxExecutor::scan(const Storage s, std::string_view left_key,
                        bool l_exclusive, std::string_view right_key,
                        bool r_exclusive, std::vector<TupleBody*>& result,
                        int64_t limit) {
  result.clear();

  std::vector<Tuple*> scan_res;
  Masstrees[get_storage(s)].scan(
      left_key.empty() ? nullptr : left_key.data(), left_key.size(),
      l_exclusive, right_key.empty() ? nullptr : right_key.data(),
      right_key.size(), r_exclusive, &scan_res, limit, callback_);

  std::set<Version*> seen;
  for (auto&& itr : scan_res) {
    // TODO: Tuple should have key? Accessing key through the latest ver is ugly
    // Must be a copy to avoid buffer overflow when changing the latest
    std::string key(itr->latest_.load(memory_order_acquire)->body_.get_key());
    SetElement<Tuple>* re = searchReadSet(s, key);
    if (re && seen.find(re->ver_) == seen.end()) {
      result.emplace_back(&(re->ver_->body_));
      seen.emplace(re->ver_);
      continue;
    }

    SetElement<Tuple>* we = searchWriteSet(s, key);
    if (we && seen.find(we->ver_) == seen.end()) {
      result.emplace_back(&(we->ver_->body_));
      seen.emplace(we->ver_);
      continue;
    }

    Version* v = read_internal(s, key, itr);
    if (this->status_ == TransactionStatus::aborted)
      return Status::ERROR_PREEMPTIVE_ABORT;
    if (v == nullptr ||
        v->status_.load(memory_order_acquire) == VersionStatus::deleted)
      continue;
    if (seen.find(v) == seen.end()) {
      result.emplace_back(&(v->body_));
      seen.emplace(v);
    }
  }

  return Status::OK;
}

/**
 * Snapshot Isolation commit:
 *   - Take cstamp.
 *   - Validate masstree node versions (phantom prevention; required for
 *     scan-based snapshot consistency).
 *   - Install writes and mark them committed.
 * No SSN anti-dependency check — that is what would upgrade SI to SSI.
 */
void TxExecutor::si_commit() {
#if ADD_ANALYSIS
  uint64_t start(rdtscp());
#endif
  this->status_ = TransactionStatus::committing;
  TransactionTable* tmt = TMT[thid_];
  tmt->status_.store(TransactionStatus::committing);

  this->cstamp_ = ++Lsn;
  tmt->cstamp_.store(this->cstamp_, memory_order_release);

  // Validate the node set (phantom prevention). SI's snapshot semantics
  // for scans require that no concurrent insert/delete have changed the
  // structure of nodes we scanned through.
  for (auto it : node_map_) {
    auto node = (MasstreeWrapper<Tuple>::node_type*) it.first;
    if (node->full_version_value() != it.second) {
      status_ = TransactionStatus::aborted;
      tmt->status_.store(TransactionStatus::aborted, memory_order_release);
      goto FINISH_SI_COMMIT;
    }
  }

  status_ = TransactionStatus::committed;
  tmt->status_.store(TransactionStatus::committed, memory_order_release);

#if ADD_ANALYSIS
  result_->local_vali_latency_ += rdtscp() - start;
  start = rdtscp();
#endif

  // Drop our reader bit on each read version.
  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
    downReadersBits((*itr).ver_);
  }

  // Install writes: stamp cstamp, mark version committed (or apply delete).
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    (*itr).ver_->cstamp_.store(this->cstamp_, memory_order_release);
#if ADD_ANALYSIS
    ++result_->local_memcpys;
#endif
    if ((*itr).op_ == OpType::DELETE) {
      Masstrees[get_storage((*itr).storage_)].remove_value((*itr).key_);
      (*itr).ver_->status_.store(VersionStatus::deleted, memory_order_release);
      gcobject_.gcq_for_record_.push_back((*itr).rcdptr_);
    } else {
      (*itr).ver_->status_.store(VersionStatus::committed,
                                 memory_order_release);
    }
    gcobject_.gcq_for_version_.emplace_back(
        GCElement((*itr).storage_, (*itr).key_, (*itr).rcdptr_, (*itr).ver_,
                  this->cstamp_));
  }
  // After pushing this commit's GCElements, expose the (possibly new)
  // queue front cstamp so other threads' gcRecord can advance safely.
  gcobject_.publishMinQueuedCstamp();

  read_set_.clear();
  write_set_.clear();
  node_map_.clear();
  TMT[thid_]->lastcstamp_.store(cstamp_, memory_order_release);

FINISH_SI_COMMIT:
#if ADD_ANALYSIS
  result_->local_commit_latency_ += rdtscp() - start;
#endif
  return;
}

/**
 * @brief function about abort.
 * clean-up local read/write set.
 * release conceptual lock.
 * @return void
 */
void TxExecutor::abort() {
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    if ((*itr).op_ == OpType::INSERT) {
      // remove inserted records
      Masstrees[get_storage((*itr).storage_)].remove_value((*itr).key_);
      delete (*itr).rcdptr_;
    }
    // SI: no successor-mark cancellation needed (we never set sstamp on writes).
    (*itr).ver_->status_.store(VersionStatus::aborted, memory_order_release);
  }
  write_set_.clear();

  /**
   * notify that this transaction finishes reading the version now.
   */
  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr)
    downReadersBits((*itr).ver_);

  read_set_.clear();
  node_map_.clear();

#if BACK_OFF

#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif

  Backoff::backoff(FLAGS_clocks_per_us);

#if ADD_ANALYSIS
  result_->local_backoff_latency_ += rdtscp() - start;
#endif

#endif
}

void TxExecutor::verify_exclusion_or_abort() {
  // SI: no SSN anti-dependency check; this is a no-op kept for API compat.
}

void TxExecutor::mainte() {
  gcstop_ = rdtscp();
  if (chkClkSpan(gcstart_, gcstop_, FLAGS_gc_inter_us * FLAGS_clocks_per_us)) {
    uint32_t loadThreshold = gcobject_.getGcThreshold();
    if (pre_gc_threshold_ != loadThreshold) {
      gcobject_.gcTMTelement(result_);
      gcobject_.gcVersion(result_);
      // gcRecord uses MinQueuedCstamp[] (published by every thread on
      // push to and pop from gcq_for_version_) to defer Tuple free
      // until no thread can still hold a reference — see the original
      // bug history in garbage_collection.cc.
      gcobject_.gcRecord();
      pre_gc_threshold_ = loadThreshold;
      gcstart_ = gcstop_;
    }
  }
}

// TODO: enable this if we want to use
// void TxExecutor::dispWS() {
//   cout << "th " << this->thid_ << " : write set : ";
//   for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
//     cout << "(" << (*itr).key_ << ", " << (*itr).ver_->val_ << "), ";
//   }
//   cout << endl;
// }

// void TxExecutor::dispRS() {
//   cout << "th " << this->thid_ << " : read set : ";
//   for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
//     cout << "(" << (*itr).key_ << ", " << (*itr).ver_->val_ << "), ";
//   }
//   cout << endl;
// }

bool TxExecutor::commit() {
  si_commit();
  if (status_ == TransactionStatus::aborted) return false;

  /**
   * Maintenance phase
   */
  mainte();
  return true;
}

bool TxExecutor::isLeader() { return this->thid_ == 0; }

void TxExecutor::leaderWork() {
  if (gcob.chkSecondRange()) {
    gcob.decideGcThreshold();
    gcob.mvSecondRangeToFirstRange();
  }
#if BACK_OFF
  leaderBackoffWork(backoff_, CCBenchResults);
#endif
}

void TxExecutor::reconnoiter_begin() { reconnoitering_ = true; }

void TxExecutor::reconnoiter_end() {
  read_set_.clear();
  node_map_.clear();
  reconnoitering_ = false;
  begin();
}

void TxScanCallback::on_resp_node(const MasstreeWrapper<Tuple>::node_type* n,
                                  uint64_t version) {
  auto it = tx_->node_map_.find((void*) n);
  if (it == tx_->node_map_.end()) {
    tx_->node_map_.emplace_hint(it, (void*) n, version);
  } else if ((*it).second != version) {
    tx_->status_ = TransactionStatus::aborted;
  }
}
