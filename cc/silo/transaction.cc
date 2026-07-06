#include <stdio.h>
#include <algorithm>
#include <string>

#include "include/atomic_tool.hh"
#include "include/log.hh"
#include "include/transaction.hh"
#include "include/scan_callback.hh"
#include "../../include/trace.hh" // izanagi: #if TRACE-guarded correctness trace

extern void displayDB();
extern void siloLeaderWork(uint64_t& epoch_timer_start,
                           uint64_t& epoch_timer_stop);

void TxExecutor::gc_records() {
  const auto r_epoch = ReclamationEpoch;

  // for records
  while (!gc_records_.empty()) {
    Tuple* rec = gc_records_.front();
    if (rec->tidword_.epoch > r_epoch) break;
    delete rec;
    gc_records_.pop_front();
  }
}

void TxExecutor::abort() {
  // remove inserted records
  for (auto& we : write_set_) {
    if (we.op_ == OpType::INSERT) {
      Masstrees[get_storage(we.storage_)].remove_value_if_present(we.key_);
      delete we.rcdptr_;
    }
  }

  gc_records();

  read_set_.clear();
  write_set_.clear();
  node_map_.clear();

#if BACK_OFF
#if ADD_ANALYSIS
  std::uint64_t start(rdtscp());
#endif

  Backoff::backoff(FLAGS_clocks_per_us);

#if ADD_ANALYSIS
  result_->local_backoff_latency_ += rdtscp() - start;
#endif
#endif
}

void TxExecutor::begin() {
  status_ = TransactionStatus::inflight;
  max_wset_.obj_ = 0;
  max_rset_.obj_ = 0;
}

// TODO: enable this if we want to use
// void TxExecutor::displayWriteSet() {
//   printf("display_write_set()\n");
//   printf("--------------------\n");
//   for (auto &ws : write_set_) {
//     printf("key\t:\t%lu\n", ws.key_);
//   }
// }

Status TxExecutor::insert(Storage s, std::string_view key, TupleBody&& body) {
#if ADD_ANALYSIS
  std::uint64_t start = rdtscp();
#endif

  if (searchWriteSet(s, key)) return Status::WARN_ALREADY_EXISTS;

  Tuple* tuple = Masstrees[get_storage(s)].get_value(key);
#if ADD_ANALYSIS
  ++result_->local_tree_traversal_;
#endif
  if (tuple != nullptr) { return Status::WARN_ALREADY_EXISTS; }

  tuple = new Tuple();
  tuple->init(std::move(body));

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

  write_set_.emplace_back(s, key, tuple, OpType::INSERT);

#if ADD_ANALYSIS
  result_->local_write_latency_ += rdtscp() - start;
#endif
  return Status::OK;
}

Status TxExecutor::delete_record(Storage s, std::string_view key) {
#if ADD_ANALYSIS
  std::uint64_t start = rdtscp();
#endif
  Tidword tidw;

  // cancel previous write
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    if ((*itr).storage_ != s) continue;
    if ((*itr).key_ == key) { write_set_.erase(itr); }
  }

  Tuple* tuple = Masstrees[get_storage(s)].get_value(key);
#if ADD_ANALYSIS
  ++result_->local_tree_traversal_;
#endif
  if (tuple == nullptr) { return Status::WARN_NOT_FOUND; }

  tidw.obj_ = loadAcquire(tuple->tidword_.obj_);
  if (tidw.absent) { return Status::WARN_NOT_FOUND; }
  write_set_.emplace_back(s, key, tuple, OpType::DELETE);

#if ADD_ANALYSIS
  result_->local_write_latency_ += rdtscp() - start;
#endif
  return Status::OK;
}

void TxExecutor::lockWriteSet() {
  Tidword expected, desired;
#if TRACE
  // Per-txn lock-coverage shadow reset (D38, 裁定7). Unconditional whole-set
  // clear at entry makes per-txn isolation independent of any abort/retry exit
  // path (defense-in-depth; the real false-green guard is the raw-lock-bit check
  // in writePhase). #if TRACE only -> compiled out of the perf build (絶対規律1).
  izanagi_trace::clear_shadow();
#endif

  [[maybe_unused]] retry
      : for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    if (itr->op_ == OpType::INSERT) continue;
    expected.obj_ = loadAcquire((*itr).rcdptr_->tidword_.obj_);
    for (;;) {
      if (expected.lock) {
#if NO_WAIT_LOCKING_IN_VALIDATION
        this->status_ = TransactionStatus::aborted;
        if (itr != write_set_.begin()) unlockWriteSet(itr);
        return;
#elif NO_WAIT_OF_TICTOC
        if (itr != write_set_.begin()) unlockWriteSet(itr);
        goto retry;
#endif
      } else {
        desired = expected;
        desired.lock = 1;
        if (compareExchange((*itr).rcdptr_->tidword_.obj_, expected.obj_,
                            desired.obj_)) {
#if TRACE
          // This worker now holds the lock on this tuple (D38). On the skeleton
          // CAS-success path (not the coder edit surface = the conflict response
          // above), so a variant cannot suppress the record without also failing
          // the acquisition it is meant to prove.
          izanagi_trace::record_lock((*itr).rcdptr_);
#endif
          break;
        }
      }
    }
    if (itr->op_ == OpType::UPDATE && itr->rcdptr_->tidword_.absent) {
      unlockWriteSet(itr);
      this->status_ = TransactionStatus::aborted;
      return;
    }

    max_wset_ = std::max(max_wset_, expected);
  }
}

Status TxExecutor::read(Storage s, std::string_view key, TupleBody** body) {
#if ADD_ANALYSIS
  std::uint64_t start = rdtscp();
#endif

  // these variable cause error (-fpermissive)
  // "crosses initialization of ..."
  // So it locate before first goto instruction.
  Tidword expected, check;
  Status stat;
  ReadElement<Tuple>* re;
  WriteElement<Tuple>* we;

  /**
   * read-own-writes or re-read from local read set.
   */
  re = searchReadSet(s, key);
  if (re) {
    *body = &(re->body_);
    goto FINISH_READ;
  }
  we = searchWriteSet(s, key);
  if (we) {
    *body = &(we->body_);
    goto FINISH_READ;
  }

  /**
   * Search tuple from data structure.
   */
  Tuple* tuple;
  tuple = Masstrees[get_storage(s)].get_value(key);
#if ADD_ANALYSIS
  ++result_->local_tree_traversal_;
#endif
  if (tuple == nullptr) return Status::WARN_NOT_FOUND;

  stat = read_internal(s, key, tuple);
  if (stat != Status::OK) { return stat; }
  *body = &(read_set_.back().body_);

FINISH_READ:
#if ADD_ANALYSIS
  result_->local_read_latency_ += rdtscp() - start;
#endif
  return Status::OK;
}

Status TxExecutor::read_internal(Storage s, std::string_view key,
                                 Tuple* tuple) {
  TupleBody body;
  Tidword expected, check;

  //(a) reads the TID word, spinning until the lock is clear

  expected.obj_ = loadAcquire(tuple->tidword_.obj_);
  // check if it is locked.
  // spinning until the lock is clear

  for (;;) {
    while (expected.lock) { expected.obj_ = loadAcquire(tuple->tidword_.obj_); }

    //(b) checks whether the record is the latest version
    // omit. because this is implemented by single version

    if (expected.absent) { return Status::WARN_NOT_FOUND; }

    //(c) reads the data
    body = TupleBody(key, tuple->body_.get_val(), tuple->body_.get_val_align());

    //(d) performs a memory fence
    // don't need.
    // order of load don't exchange.

    //(e) checks the TID word again
    check.obj_ = loadAcquire(tuple->tidword_.obj_);
    if (expected == check) break;
    expected = check;
#if ADD_ANALYSIS
    ++result_->local_extra_reads_;
#endif
  }
  read_set_.emplace_back(s, key, tuple, std::move(body), expected);
#if SLEEP_READ_PHASE
  sleepTics(SLEEP_READ_PHASE);
#endif

  return Status::OK;
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
  auto rset_init_size = read_set_.size();

  std::vector<Tuple*> scan_res;
  Masstrees[get_storage(s)].scan(
      left_key.empty() ? nullptr : left_key.data(), left_key.size(),
      l_exclusive, right_key.empty() ? nullptr : right_key.data(),
      right_key.size(), r_exclusive, &scan_res, limit, callback_);

  for (auto&& itr : scan_res) {
    ReadElement<Tuple>* re = searchReadSet(s, itr->body_.get_key());
    if (re) {
      result.emplace_back(&(re->body_));
      continue;
    }

    WriteElement<Tuple>* we = searchWriteSet(s, itr->body_.get_key());
    if (we) {
      result.emplace_back(&(we->body_));
      continue;
    }

    Status stat = read_internal(s, itr->body_.get_key(), itr);
    if (stat != Status::OK && stat != Status::WARN_NOT_FOUND) { return stat; }
  }

  if (rset_init_size != read_set_.size()) {
    for (auto itr = read_set_.begin() + rset_init_size; itr != read_set_.end();
         ++itr) {
      result.emplace_back(&((*itr).body_));
    }
  }

  return Status::OK;
}

void tx_delete([[maybe_unused]] std::uint64_t key) {}

ReadElement<Tuple>* TxExecutor::searchReadSet(Storage s, std::string_view key) {
  for (auto& re : read_set_) {
    if (re.storage_ != s) continue;
    if (re.key_ == key) return &re;
  }

  return nullptr;
}

WriteElement<Tuple>* TxExecutor::searchWriteSet(Storage s,
                                                std::string_view key) {
  for (auto& we : write_set_) {
    if (we.storage_ != s) continue;
    if (we.key_ == key) return &we;
  }

  return nullptr;
}

void TxExecutor::unlockWriteSet() {
  Tidword expected, desired;

  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    if ((*itr).op_ == OpType::INSERT) continue;
    expected.obj_ = loadAcquire((*itr).rcdptr_->tidword_.obj_);
    desired = expected;
    desired.lock = 0;
    storeRelease((*itr).rcdptr_->tidword_.obj_, desired.obj_);
  }
#if TRACE
  izanagi_trace::clear_shadow();  // abort/retry exit -> reset coverage shadow (D38, 裁定7)
#endif
}

void TxExecutor::unlockWriteSet(
    std::vector<WriteElement<Tuple>>::iterator end) {
  Tidword expected, desired;

  for (auto itr = write_set_.begin(); itr != end; ++itr) {
    if ((*itr).op_ == OpType::INSERT) continue;
    expected.obj_ = loadAcquire((*itr).rcdptr_->tidword_.obj_);
    desired = expected;
    desired.lock = 0;
    storeRelease((*itr).rcdptr_->tidword_.obj_, desired.obj_);
  }
#if TRACE
  izanagi_trace::clear_shadow();  // partial unlock (retry/abort) -> reset shadow (D38, 裁定7)
#endif
}

bool TxExecutor::validationPhase() { // Validation Phase
#if ADD_ANALYSIS
  std::uint64_t start = rdtscp();
#endif

  /* Phase 1
   * lock write_set_ sorted.*/
  sort(write_set_.begin(), write_set_.end());
  lockWriteSet();
  if (this->status_ == TransactionStatus::aborted) return false;

  asm volatile("" ::: "memory");
  atomicStoreThLocalEpoch(thid_, atomicLoadGE());
  asm volatile("" ::: "memory");

  /* Phase 2 abort if any condition of below is satisfied.
   * 1. tid of read_set_ changed from it that was got in Read Phase.
   * 2. not latest version
   * 3. the tuple is locked and it isn't included by its write set.*/

  Tidword check;
  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
    // 1
    check.obj_ = loadAcquire((*itr).rcdptr_->tidword_.obj_);
    if ((*itr).get_tidword().epoch != check.epoch ||
        (*itr).get_tidword().tid != check.tid) {
#if ADD_ANALYSIS
      result_->local_vali_latency_ += rdtscp() - start;
#endif
      this->status_ = TransactionStatus::aborted;
      unlockWriteSet();
      return false;
    }
    // 2
    // if (!check.latest) return false;

    // 3
    if (check.lock && !searchWriteSet((*itr).storage_, (*itr).key_)) {
#if ADD_ANALYSIS
      result_->local_vali_latency_ += rdtscp() - start;
#endif
      this->status_ = TransactionStatus::aborted;
      unlockWriteSet();
      return false;
    }
    max_rset_ = std::max(max_rset_, check);
  }

  // node validation
  for (auto it : node_map_) {
    auto node = (MasstreeWrapper<Tuple>::node_type*) it.first;
    if (node->full_version_value() != it.second) {
      this->status_ = TransactionStatus::aborted;
      unlockWriteSet();
      return false;
    }
  }

  // goto Phase 3
#if ADD_ANALYSIS
  result_->local_vali_latency_ += rdtscp() - start;
#endif
  this->status_ = TransactionStatus::committed;
  return true;
}

void TxExecutor::wal(std::uint64_t ctid) {
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    LogRecord log(ctid, (*itr).key_, (char*) "FIXME"); // TODO: logging
    log_set_.emplace_back(log);
    latest_log_header_.chkSum_ += log.computeChkSum();
    ++latest_log_header_.logRecNum_;
  }

  if (log_set_.size() > LOGSET_SIZE / 2) {
    // prepare write header
    latest_log_header_.convertChkSumIntoComplementOnTwo();

    // write header
    logfile_.write((void*) &latest_log_header_, sizeof(LogHeader));

    // write log record
    // for (auto itr = log_set_.begin(); itr != log_set_.end(); ++itr)
    //  logfile_.write((void *)&(*itr), sizeof(LogRecord));
    logfile_.write((void*) &(log_set_[0]),
                   sizeof(LogRecord) * latest_log_header_.logRecNum_);

    // logfile_.fdatasync();

    // clear for next transactions.
    latest_log_header_.init();
    log_set_.clear();
  }
}

Status TxExecutor::update(Storage s, std::string_view key, TupleBody&& body) {
#if ADD_ANALYSIS
  std::uint64_t start = rdtscp();
#endif

  if (searchWriteSet(s, key)) goto FINISH_WRITE;

  /**
   * Search tuple from data structure.
   */
  Tuple* tuple;
  ReadElement<Tuple>* re;
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

  write_set_.emplace_back(s, key, tuple, std::move(body), OpType::UPDATE);

FINISH_WRITE:

#if ADD_ANALYSIS
  result_->local_write_latency_ += rdtscp() - start;
#endif
  return Status::OK;
}

void TxExecutor::writePhase() {
  // It calculates the smallest number that is
  //(a) larger than the TID of any record read or written by the transaction,
  //(b) larger than the worker's most recently chosen TID,
  // and (C) in the current global epoch.

  Tidword tid_a, tid_b, tid_c;

  // calculates (a)
  // about read_set_
  tid_a = std::max(max_wset_, max_rset_);
  tid_a.tid++;

  // calculates (b)
  // larger than the worker's most recently chosen TID,
  tid_b = mrctid_;
  tid_b.tid++;

  // calculates (c)
  tid_c.epoch = ThLocalEpoch[thid_].obj_;

  // compare a, b, c
  Tidword maxtid = std::max({tid_a, tid_b, tid_c});
  maxtid.lock = 0;
  maxtid.latest = 1;
  mrctid_ = maxtid;

#if TRACE
  // izanagi correctness trace (observer-effect isolated; compiled out when
  // TRACE==0). maxtid is this txn's serialization point AND the new version
  // stamped into every written tuple, so (epoch,tid) is both the commit-order
  // key and the produced version id. read_set_/write_set_ are still intact
  // here (cleared at the end of writePhase). All data is CC-native -- no
  // verification-only tuple field is added (docs/ccbench-anatomy.md  4-5).
  // txid is hoisted to writePhase scope (D38) so the per-storeRelease retention
  // check in the write loop below shares this txn's id (keeps X lines correlated
  // with this txn's C/R/W; a fresh next_txid() would orphan them).
  const std::uint64_t izanagi_txid = izanagi_trace::next_txid();
  izanagi_trace::emit_commit(thid_, izanagi_txid, maxtid.epoch, maxtid.tid);
  for (auto& re : read_set_) {
    const Tidword v = re.get_tidword();
    izanagi_trace::emit_read(thid_, izanagi_txid, izanagi_trace::key_to_hex(re.key_),
                             v.epoch, v.tid);
  }
  for (auto& we : write_set_) {
    const char op = (we.op_ == OpType::INSERT)   ? 'I'
                    : (we.op_ == OpType::DELETE) ? 'D'
                                                 : 'U';
    izanagi_trace::emit_write(thid_, izanagi_txid, izanagi_trace::key_to_hex(we.key_),
                              op, maxtid.epoch, maxtid.tid);
  }
  // Entry lock-coverage check (D38, 裁定4 point 1 = acquisition coverage).
  // Every non-INSERT write must be covered, right now at writePhase entry, by a
  // lock THIS worker holds. Reads the live tidword.lock (ground truth in the
  // tuple) AND the shadow set (owner = us; tidword has no owner field). Emits X
  // (not-locked-at-entry) per uncovered write = catches lock-skip. INSERT excluded
  // (created lock=1/absent, covered by construction).
  for (auto& we : write_set_) {
    if (we.op_ == OpType::INSERT) continue;
    Tidword cur;
    cur.obj_ = loadAcquire(we.rcdptr_->tidword_.obj_);
    if (!cur.lock || !izanagi_trace::holds_lock(we.rcdptr_)) {
      izanagi_trace::emit_lock_violation(thid_, izanagi_txid,
                                         izanagi_trace::key_to_hex(we.key_),
                                         "not-locked-at-entry");
    }
  }
#endif

#if WAL
  wal(maxtid.obj_);
#endif

  // write(record, commit-tid)
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    // update and unlock
    switch ((*itr).op_) {
      case OpType::UPDATE: {
#if TRACE
        // Retention check (D38, 裁定4 point 2): the lock must still be held right
        // before we write this tuple's data. An early unlock (lock released before
        // the memcpy) opens a torn-read window -> emit X (lock-lost-before-write).
        {
          Tidword cur;
          cur.obj_ = loadAcquire((*itr).rcdptr_->tidword_.obj_);
          if (!cur.lock)
            izanagi_trace::emit_lock_violation(
                thid_, izanagi_txid, izanagi_trace::key_to_hex((*itr).key_),
                "lock-lost-before-write");
        }
#endif
        memcpy((*itr).rcdptr_->body_.get_val_ptr(), (*itr).body_.get_val_ptr(),
               (*itr).body_.get_val_size());
        storeRelease((*itr).rcdptr_->tidword_.obj_, maxtid.obj_);
        break;
      }
      case OpType::INSERT: {
        maxtid.absent = false;
        storeRelease((*itr).rcdptr_->tidword_.obj_, maxtid.obj_);
        break;
      }
      case OpType::DELETE: {
#if TRACE
        {
          Tidword cur;
          cur.obj_ = loadAcquire((*itr).rcdptr_->tidword_.obj_);
          if (!cur.lock)
            izanagi_trace::emit_lock_violation(
                thid_, izanagi_txid, izanagi_trace::key_to_hex((*itr).key_),
                "lock-lost-before-write");
        }
#endif
        maxtid.absent = true;
        // Return value intentionally ignored: a missing key still needs the
        // tid bump and gc_records_ push below.
        Masstrees[get_storage((*itr).storage_)].remove_value_if_present(
            (*itr).key_);
        storeRelease((*itr).rcdptr_->tidword_.obj_, maxtid.obj_);
        // create information for garbage collection
        gc_records_.push_back((*itr).rcdptr_);
        break;
      }
      default:
        ERR;
    }
  }

#if TRACE
  izanagi_trace::clear_shadow();  // success path -> reset coverage shadow (D38, 裁定7)
#endif
  gc_records();
  read_set_.clear();
  write_set_.clear();
  node_map_.clear();
}

bool TxExecutor::commit() {
  if (validationPhase()) {
    writePhase();
    return true;
  } else {
    return false;
  }
}

bool TxExecutor::isLeader() { return this->thid_ == 0; }

void TxExecutor::leaderWork() {
  siloLeaderWork(this->epoch_timer_start, this->epoch_timer_stop);
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
