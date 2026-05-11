
#include <stdio.h>
#include <xmmintrin.h>

#include <fstream>
#include <string>
#include <vector>

#include "include/common.hh"
#include "include/transaction.hh"
#include "include/version.hh"

extern bool chkClkSpan(const uint64_t start, const uint64_t stop, const uint64_t threshold);
extern void displaySLogSet();
extern void displayDB();
extern void mvtoLeaderWork();
extern std::vector<Result> MvtoResult;

using namespace std;

#define MSK_TID 0b11111111

/**
 * @brief Initialize function of transaction.
 * Allocate timestamp.
 * @return void
 */
void TxExecutor::begin() {
  /**
   * Allocate timestamp in read phase.
   */
  this->status_ = TransactionStatus::inflight;
  this->wts_.generateTimeStamp(thid_);
  __atomic_store_n(&(ThreadWtsArray[thid_].obj_), this->wts_.ts_, __ATOMIC_RELEASE);
  this->rts_ = MinWts.load(std::memory_order_acquire) - 1;
  __atomic_store_n(&(ThreadRtsArray[thid_].obj_), this->rts_, __ATOMIC_RELEASE);
}

Version *TxExecutor::read_internal(Storage s, std::string_view key, Tuple *tuple) {
  Version *ver, *later_ver;
  later_ver = nullptr;

  // Choose the base timestamp (use rts for snapshot read).
  uint64_t ts = this->is_ronly_ ? this->rts_ : this->wts_.ts_;

  ver = get_latest_previous_version(ts, tuple->ldAcqLatest(), &later_ver);
  if (ver == nullptr) {
    return nullptr;
  } else {
    update_rts(ver);
  }

  if (ver->ldAcqStatus() == VersionStatus::deleted) {
    return nullptr;
  }
  read_set_.emplace_back(s, key, tuple, later_ver, ver);

  return ver;
}

/**
 * @brief Transaction read function.
 * @param [in] key The key of key-value
 */
Status TxExecutor::read(Storage s, std::string_view key, TupleBody **body) {
#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif  // if ADD_ANALYSIS

  Version *ver;
  ReadElement<Tuple> *re;
  WriteElement<Tuple> *we;

  /**
   * read-own-writes or re-read from local read set.
   */
  re = searchReadSet(s, key);
  if (re) {
    *body = &(re->ver_->body_);
    goto FINISH_READ;
  }
  we = searchWriteSet(s, key);
  if (we) {
    *body = &(we->new_ver_->body_);
    goto FINISH_READ;
  }

  /**
   * Search versions from data structure.
   */
  Tuple *tuple;
  tuple = Masstrees[get_storage(s)].get_value(key);
#if ADD_ANALYSIS
  ++result_->local_tree_traversal_;
#endif  // if ADD_ANALYSIS
  if (tuple == nullptr) return Status::WARN_NOT_FOUND;

  ver = read_internal(s, key, tuple);
  if (ver == nullptr) return Status::WARN_NOT_FOUND;

  /**
   * Read payload.
   */
  *body = &(ver->body_);

  FINISH_READ:
#if ADD_ANALYSIS
  result_->local_read_latency_ += rdtscp() - start;
#endif
  return Status::OK;
}

/**
 * @brief Transaction write function.
 * @param [in] key The key of key-value
 */
Status TxExecutor::write(Storage s, std::string_view key, TupleBody &&body) {
#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif  // if ADD_ANALYSIS
  if (searchWriteSet(s, key)) goto FINISH_WRITE;

  Tuple *tuple;
  bool rmw;
  rmw = false;
  ReadElement <Tuple> *re;
  re = searchReadSet(s, key);
  if (re) {
    rmw = true;
    tuple = re->rcdptr_;
  } else {
    tuple = Masstrees[get_storage(s)].get_value(key);
#if ADD_ANALYSIS
    ++result_->local_tree_traversal_;
#endif  // if ADD_ANALYSIS
    if (tuple == nullptr) return Status::WARN_NOT_FOUND;
  }

  // Install new version with pending state
  Version *ver, *new_ver;
  new_ver = newVersionGeneration(tuple, std::move(body));
  if (FLAGS_preserve_write) {
    install_version(tuple, new_ver);
  }

  write_set_.emplace_back(
      s, key, tuple, nullptr, new_ver, rmw ? OpType::RMW : OpType::UPDATE);

  FINISH_WRITE:
#if ADD_ANALYSIS
  result_->local_write_latency_ += rdtscp() - start;
#endif  // if ADD_ANALYSIS
  return Status::OK;
}

Status TxExecutor::insert(Storage s, std::string_view key, TupleBody &&body) {
#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif  // if ADD_ANALYSIS

  if (searchWriteSet(s, key)) return Status::WARN_ALREADY_EXISTS;

  Tuple *tuple = Masstrees[get_storage(s)].get_value(key);
#if ADD_ANALYSIS
  ++result_->local_tree_traversal_;
#endif
  if (tuple != nullptr) {
    return Status::WARN_ALREADY_EXISTS;
  }

  tuple = new Tuple();
  Version *new_ver = newVersionGeneration(tuple, std::move(body));
  tuple->init(this->thid_, new_ver, this->wts_.ts_, std::move(body));

  Status stat = Masstrees[get_storage(s)].insert_value(key, tuple);
  if (stat == Status::WARN_ALREADY_EXISTS) {
    delete tuple;
    return stat;
  }

  write_set_.emplace_back(s, key, tuple, new_ver, OpType::INSERT);

#if ADD_ANALYSIS
  result_->local_write_latency_ += rdtscp() - start;
#endif  // if ADD_ANALYSIS
  return Status::OK;
}

Status TxExecutor::delete_record(Storage s, std::string_view key) {
#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif  // if ADD_ANALYSIS

  // cancel previous write
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    if ((*itr).storage_ != s) continue;
    if ((*itr).key_ == key) {
      write_set_.erase(itr);
    }
  }

  Tuple *tuple;
  ReadElement<Tuple> *re;
  re = searchReadSet(s, key);
  if (re) {
    tuple = re->rcdptr_;
  } else {
    tuple = Masstrees[get_storage(s)].get_value(key);
#if ADD_ANALYSIS
    ++result_->local_tree_traversal_;
#endif  // if ADD_ANALYSIS
    if (tuple == nullptr) return Status::WARN_NOT_FOUND;
  }

  // Install delete version with pending state
  Version *ver, *new_ver;
  new_ver = new Version(this->wts_.ts_);
  if (FLAGS_preserve_write) {
    install_version(tuple, new_ver);
  }

  write_set_.emplace_back(s, key, tuple, nullptr, new_ver, OpType::DELETE);

  FINISH_DELETE:
#if ADD_ANALYSIS
  result_->local_write_latency_ += rdtscp() - start;
#endif  // if ADD_ANALYSIS
  return Status::OK;
}

Status TxExecutor::scan(const Storage s,
                        std::string_view left_key, bool l_exclusive,
                        std::string_view right_key, bool r_exclusive,
                        std::vector<TupleBody *> &result) {
  return scan(s, left_key, l_exclusive, right_key, r_exclusive, result, -1);
}

Status TxExecutor::scan(const Storage s,
                        std::string_view left_key, bool l_exclusive,
                        std::string_view right_key, bool r_exclusive,
                        std::vector<TupleBody *> &result, int64_t limit) {
  result.clear();
  auto rset_init_size = read_set_.size();

  std::vector<Tuple *> scan_res;
  Masstrees[get_storage(s)].scan(
      left_key.empty() ? nullptr : left_key.data(), left_key.size(),
      l_exclusive, right_key.empty() ? nullptr : right_key.data(),
      right_key.size(), r_exclusive, &scan_res, limit);

  for (auto &&itr: scan_res) {
    // TODO: Tuple should have key? Accessing key through the latest ver is ugly
    // Must be a copy to avoid buffer overflow when changing the latest
    std::string key(itr->latest_.load(memory_order_acquire)->body_.get_key());
    ReadElement<Tuple> *re = searchReadSet(s, key);
    if (re) {
      result.emplace_back(&(re->ver_->body_));
      continue;
    }

    WriteElement<Tuple> *we = searchWriteSet(s, std::string_view(key));
    if (we) {
      result.emplace_back(&(we->new_ver_->body_));
      continue;
    }

    Version *v = read_internal(s, key, itr);
    if (this->status_ == TransactionStatus::aborted)
      return Status::ERROR_PREEMPTIVE_ABORT;
  }

  if (rset_init_size != read_set_.size()) {
    for (auto itr = read_set_.begin() + rset_init_size;
         itr != read_set_.end(); ++itr) {
      result.emplace_back(&((*itr).ver_->body_));
    }
  }

  return Status::OK;
}

bool TxExecutor::validation() {
#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif  // if ADD_ANALYSIS
  bool result(true);

  // Install versions
  if (!FLAGS_preserve_write) {
    for (auto &we : write_set_) {
      if (we.op_ == OpType::INSERT) {
        continue;
      }
      install_version(we.rcdptr_, we.new_ver_);
    }
  }

  /**
   * version consistency check
   *(a) every previously visible version v of the records in the read set is the
   * currently visible version to the transaction.
   */
  for (auto &re : read_set_) {
    Version *ver = re.later_ver_ ? re.later_ver_ : re.rcdptr_->ldAcqLatest();
    ver = get_latest_previous_version(ver);
    /**
     * This part is different from the original.
     * Checking that view is not broken.
     * The original implementation is buggy because it does not have this.
     */
    if (re.ver_ != ver) {
      result = false;
      goto FINISH_VALIDATION;
    }
  }

  /**
   * (b) every currently visible version v of the records in the write set
   * satisfies (v.rts) <= (tx.ts)
   */
  for (auto &we : write_set_) {
    if (we.op_ == OpType::INSERT) {
      continue;
    }
    Version *ver = get_latest_previous_version(we.new_ver_);
    if (ver->ldAcqRts() > this->wts_.ts_ || ver->ldAcqStatus() == VersionStatus::deleted) {
      result = false;
      goto FINISH_VALIDATION;
    }
  }

  FINISH_VALIDATION:
#if ADD_ANALYSIS
  result_->local_vali_latency_ += rdtscp() - start;
#endif  // if ADD_ANALYSIS
  return result;
}

inline void TxExecutor::commit_pending_versions() {
  for (auto &we : write_set_) {
    if (we.op_ == OpType::DELETE) {
      Masstrees[get_storage(we.storage_)].remove_value(we.key_);
      we.new_ver_->status_.store(VersionStatus::deleted, std::memory_order_release);
      gc_records_.push_back(we.rcdptr_);
    } else {
      we.new_ver_->status_.store(VersionStatus::committed, std::memory_order_release);
    }
    gcq_.emplace_back(
        GCElement(
            we.storage_, we.key_, we.rcdptr_, we.new_ver_, this->wts_.ts_));
  }
}

/**
 * @brief function about abort.
 * clean-up local read/write set.
 * @return void
 */
void TxExecutor::abort() {
  clean_up_read_write_set();
  maintenance();
#if BACK_OFF
  backoff();
#endif
}

void TxExecutor::gc_versions() {
  while (!gcq_.empty()) {
    if (gcq_.front().wts_ >= MinRts.load(memory_order_acquire)) break;

    /*
      * (a) acquiring the garbage collection lock succeeds
      * thid_+1 : leader thread id is 0.
      * so, if we get right by id 0, other worker thread can't detect.
      */
    if (gcq_.front().rcdptr_->getGCRight(thid_ + 1) == false) {
      // fail acquiring the lock
      gcq_.pop_front();
      continue;
    }

    Tuple *tuple = gcq_.front().rcdptr_;
    // (b) v.wts > record.min_wts
    if (gcq_.front().wts_ <= tuple->min_wts_) {
      // releases the lock
      tuple->returnGCRight();
      gcq_.pop_front();
      continue;
    }
    // this pointer may be dangling.

    Version *delTarget =
        gcq_.front().ver_->next_.load(std::memory_order_acquire);

    // the thread detaches the rest of the version list from v
    gcq_.front().ver_->next_.store(nullptr, std::memory_order_release);
    // updates record.min_wts
    tuple->min_wts_.store(gcq_.front().ver_->wts_, memory_order_release);
    gcAfterThisVersion(tuple, delTarget);
    // releases the lock
    tuple->returnGCRight();
    gcq_.pop_front();
  }
}

void TxExecutor::gc_records() {
  // TODO: GC condition // const auto r_epoch = ReclamationEpoch;

  // for records
  while (!gc_records_.empty()) {
    Tuple *rec = gc_records_.front();
    Version *latest = rec->ldAcqLatest();
    if (latest->ldAcqWts() >= MinRts.load(memory_order_acquire)) break;
    if (latest->ldAcqStatus() != VersionStatus::deleted) ERR;
    delete rec;
    gc_records_.pop_front();
  }
}

void TxExecutor::maintenance() {
  /*Maintenance
   * Schedule garbage collection
   * Declare quiescent state
   * Collect garbage created by prior transactions
   * バージョンリストにおいてMinRtsよりも古いバージョンの中でコミット済み最新のもの
   * 以外のバージョンは全てデリート可能。絶対に到達されないことが保証される.
   */
#if ADD_ANALYSIS
  uint64_t start;
  start = rdtscp();
#endif
  //-----
  if (__atomic_load_n(&(GCExecuteFlag[thid_].obj_), __ATOMIC_ACQUIRE) == 1) {
#if ADD_ANALYSIS
    ++result_->local_gc_counts_;
#endif
    gc_versions();
    gc_records();

    __atomic_store_n(&(GCExecuteFlag[thid_].obj_), 0, __ATOMIC_RELEASE);
  }

  this->gcstop_ = rdtscp();
  if (chkClkSpan(this->gcstart_, this->gcstop_, FLAGS_gc_inter_us * FLAGS_clocks_per_us) &&
      (loadAcquire(GCFlag[thid_].obj_) == 0)) {
    storeRelease(GCFlag[thid_].obj_, 1);
    this->gcstart_ = this->gcstop_;
  }
  //-----
#if ADD_ANALYSIS
  result_->local_gc_latency_ += rdtscp() - start;
#endif
}

void TxExecutor::writePhase() {
#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif
  commit_pending_versions();
  read_set_.clear();
  write_set_.clear();
#if ADD_ANALYSIS
  result_->local_commit_latency_ += rdtscp() - start;
#endif
}

bool TxExecutor::commit() {
  /**
   * Excerpt from original paper 3.1 Multi-Clocks Timestamp Allocation
   * A read-only transaction uses (thread.rts) instead, 
   * and does not track or validate the read set; 
   */
  if (this->is_ronly_) {
    read_set_.clear();
    return true;
  } else {
    /**
     * Validation phase
     */
    if (!validation()) {
      return false;
    }

    /**
     * Write phase
     */
    writePhase();

    /**
     * Maintenance phase
     */
    maintenance();
    return true;
  }
}

bool TxExecutor::isLeader() {
  return this->thid_ == 0;
}

void TxExecutor::leaderWork() {
  mvtoLeaderWork();
#if BACK_OFF
  leaderBackoffWork(backoff_, MvtoResult);
#endif
}

void TxExecutor::reconnoiter_begin() {
  reconnoitering_ = true;
}

void TxExecutor::reconnoiter_end() {
  read_set_.clear();
  reconnoitering_ = false;
  begin();
}
