#include <stdio.h>
#include <xmmintrin.h>

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "include/common.hh"
#include "include/scan_callback.hh"
#include "include/transaction.hh"
#include "include/version.hh"

#include "../include/backoff.hh"
#include "../include/debug.hh"
#include "../include/masstree_wrapper.hh"
#include "../include/tsc.hh"

extern bool chkClkSpan(const uint64_t start, const uint64_t stop, const uint64_t threshold);
extern void displaySLogSet();
extern void displayDB();
extern void cicadaLeaderWork();
extern std::vector<Result> CicadaResult;

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
  __atomic_store_n(&(ThreadWtsArray[thid_].obj_), this->wts_.ts_,
                   __ATOMIC_RELEASE);
  this->rts_ = MinWts.load(std::memory_order_acquire) - 1;
  __atomic_store_n(&(ThreadRtsArray[thid_].obj_), this->rts_, __ATOMIC_RELEASE);

  /* one-sided synchronization
   * tanabe... disabled.
   * When the database size is small that all record can be on
   * cache, remote worker's clock can also be on cache after one-sided
   * synchronization.
   * After that, by transaction processing, cache line invalidation
   * often occurs and degrade throughput.
   * If you set any size of database,
   * it can't improve performance of tps.
   * It is for progress-guarantee or fairness or like these.
   *
  stop = rdtscp();
  if (chkClkSpan(start, stop, 100 * FLAGS_clocks_per_us)) {
    uint64_t maxwts;
      maxwts = __atomic_load_n(&(ThreadWtsArray[1].obj_), __ATOMIC_ACQUIRE);
    //record the fastest one, and adjust it.
    //one-sided synchronization
    uint64_t check;
    for (unsigned int i = 2; i < THREAD_NUM; ++i) {
      check = __atomic_load_n(&(ThreadWtsArray[i].obj_), __ATOMIC_ACQUIRE);
      if (maxwts < check) maxwts = check;
    }
    maxwts = maxwts & (~MSK_TID);
    maxwts = maxwts | thid_;
    this->wts_.ts = maxwts;
    __atomic_store_n(&(ThreadWtsArray[thid_].obj_), this->wts_.ts,
  __ATOMIC_RELEASE);

    //modify the start position of stopwatch.
    start = stop;
  }
  */
}

Version* TxExecutor::read_internal(Storage s, std::string_view key, Tuple* tuple) {
  // Search version
  Version *ver, *later_ver;
  later_ver = nullptr;

#if SINGLE_EXEC
  ver = &tuple->inline_ver_;
#else

  /**
   * Choose the correct timestamp from write timestamp and read timestamp.
   */
  uint64_t trts;
  if (this->is_ronly_) {
    trts = this->rts_;
  } else {
    trts = this->wts_.ts_;
  }

  /**
   * Scan to the area to be viewed
   */
  ver = tuple->ldAcqLatest();
  while (ver->ldAcqWts() > trts) {
    later_ver = ver;
    ver = ver->ldAcqNext();
    if (ver == nullptr) return nullptr;
  }
  while (ver->status_.load(memory_order_acquire) != VersionStatus::committed
    && ver->status_.load(memory_order_acquire) != VersionStatus::deleted) {
    /**
     * Wait for the result of the pending version in the view.
     */
    while (ver->status_.load(memory_order_acquire) == VersionStatus::pending) {
    }
    if (ver->status_.load(memory_order_acquire) == VersionStatus::aborted) {
      ver = ver->ldAcqNext();
    }
    if (ver == nullptr) return nullptr;
  }
#endif

  if (ver->ldAcqStatus() == VersionStatus::deleted) {
    // TODO: try another version?
    return nullptr;
  }

  read_set_.emplace_back(s, key, tuple, later_ver, ver);

#if INLINE_VERSION_OPT
#if INLINE_VERSION_PROMOTION
#if ADD_ANALYSIS
  result_->local_read_latency_ += rdtscp() - start;
#endif  // if ADD_ANALYSIS
  inlineVersionPromotion(s, key, tuple, later_ver, ver);
#endif  // if INLINE_VERSION_PROMOTION
#endif  // if INLINE_VERSION_OPT

  return ver;
}

/**
 * @brief Transaction read function.
 * @param [in] key The key of key-value
 */
Status TxExecutor::read(Storage s, std::string_view key, TupleBody** body) {
#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif  // if ADD_ANALYSIS

  Version* ver;
  ReadElement<Tuple>* re;
  WriteElement<Tuple>* we;

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
Status TxExecutor::write(Storage s, std::string_view key, TupleBody&& body) {
#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif  // if ADD_ANALYSIS

  /**
   * Update  from local write set.
   * Special treat due to performance.
   */
  if (searchWriteSet(s, key)) goto FINISH_WRITE;

  Tuple *tuple;
  bool rmw;
  rmw = false;
  ReadElement<Tuple> *re;
  re = searchReadSet(s, key);
  if (re) {
    /**
     * If it can find record in read set, use this for high performance.
     */
    rmw = true;
    tuple = re->rcdptr_;
    /* Now, it is difficult to use re->later_ver_ by simple customize.
     * Because if this write is from inline version promotion which is from read
     * only tx, the re->later_ver is suitable for read only search by read only
     * timestamp. Of course, it is unsuitable for search for write. It is high
     * cost to consider these things.
     * I try many somethings but it can't improve performance. cost > profit.*/
  } else {
    /**
     * Search record from data structure.
     */
    tuple = Masstrees[get_storage(s)].get_value(key);
#if ADD_ANALYSIS
    ++result_->local_tree_traversal_;
#endif  // if ADD_ANALYSIS
    if (tuple == nullptr) return Status::WARN_NOT_FOUND;
  }

#if SINGLE_EXEC
  write_set_.emplace_back(s, key, tuple, nullptr, &tuple->inline_ver_, rmw);
#else
  Version *later_ver, *ver;
  later_ver = nullptr;
  ver = tuple->ldAcqLatest();

  if (rmw || WRITE_LATEST_ONLY) {
    /**
     * Search version (for early abort check)
     * search latest (committed or pending) version and use for early abort
     * check pending version may be committed, so use for early abort check.
     */

    /**
     * Early abort check(EAC)
     * here, version->status is commit or pending.
     */
    if (ver->wts_.load(memory_order_acquire) > this->wts_.ts_) {
      /**
       * if pending, it don't have to be aborted.
       * But it may be aborted, so early abort.
       * if committed, it must be aborted in validaiton phase due to order of
       * newest to oldest.
       */
      this->status_ = TransactionStatus::aborted;
      goto FINISH_WRITE;
    }
  } else {
    /**
     * not rmw
     */
    while (ver->wts_.load(memory_order_acquire) > this->wts_.ts_) {
      later_ver = ver;
      ver = ver->ldAcqNext();
    }
  }

  /**
   * Constraint from new to old.
   */
  if ((ver->ldAcqRts() > this->wts_.ts_) &&
      (ver->ldAcqStatus() == VersionStatus::committed)) {
    /**
     * It must be aborted in validation phase, so early abort.
     */
    this->status_ = TransactionStatus::aborted;
    goto FINISH_WRITE;
  }

  Version *new_ver;
  new_ver = newVersionGeneration(tuple, std::move(body));
  write_set_.emplace_back(s, key, tuple, later_ver, new_ver, rmw? OpType::RMW : OpType::UPDATE);
#endif  // if SINGLE_EXEC

FINISH_WRITE:

#if ADD_ANALYSIS
  result_->local_write_latency_ += rdtscp() - start;
#endif  // if ADD_ANALYSIS

  return Status::OK;
}

Status TxExecutor::insert(Storage s, std::string_view key, TupleBody&& body) {
#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif  // if ADD_ANALYSIS

  if (searchWriteSet(s, key)) return Status::WARN_ALREADY_EXISTS;

  Tuple* tuple = Masstrees[get_storage(s)].get_value(key);
#if ADD_ANALYSIS
  ++result_->local_tree_traversal_;
#endif
  if (tuple != nullptr) {
    return Status::WARN_ALREADY_EXISTS;
  }

  tuple = new Tuple();
  Version *new_ver = newVersionGeneration(tuple, std::move(body));
  tuple->init(this->thid_, new_ver, this->wts_.ts_, std::move(body));

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

  Tuple* tuple;
  bool rmw;
  rmw = false;
  ReadElement<Tuple> *re;
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

  // Delete latest only for simplicity
  Version *later_ver, *ver;
  later_ver = nullptr;
  ver = tuple->ldAcqLatest();
  if (ver->wts_.load(memory_order_acquire) > this->wts_.ts_) {
    /**
     * if pending, it don't have to be aborted.
     * But it may be aborted, so early abort.
     * if committed, it must be aborted in validaiton phase due to order of
     * newest to oldest.
     */
    this->status_ = TransactionStatus::aborted;
    goto FINISH_DELETE;
  }

  /**
   * Constraint from new to old.
   */
  if ((ver->ldAcqRts() > this->wts_.ts_) &&
      (ver->ldAcqStatus() == VersionStatus::committed)) {
    /**
     * It must be aborted in validation phase, so early abort.
     */
    this->status_ = TransactionStatus::aborted;
    goto FINISH_DELETE;
  }

  Version *new_ver;
  new_ver = new Version(this->wts_.ts_);
  write_set_.emplace_back(s, key, tuple, later_ver, new_ver, OpType::DELETE);

FINISH_DELETE:
#if ADD_ANALYSIS
  result_->local_write_latency_ += rdtscp() - start;
#endif  // if ADD_ANALYSIS
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
    // TODO: Tuple should have key? Accessing key through the latest ver is ugly
    // Must be a copy to avoid buffer overflow when changing the latest
    std::string key(itr->latest_.load(memory_order_acquire)->body_.get_key());
    ReadElement<Tuple>* re = searchReadSet(s, key);
    if (re) {
      result.emplace_back(&(re->ver_->body_));
      continue;
    }

    WriteElement<Tuple>* we = searchWriteSet(s, std::string_view(key));
    if (we) {
      result.emplace_back(&(we->new_ver_->body_));
      continue;
    }

    Version* v = read_internal(s, key, itr);
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

  /**
   * Sort write set by contention.
   * Pre-check version consistency.
   */
  bool result(true);
  if (!precheckInValidation()) {
    result = false;
    goto FINISH_VALIDATION;
  }

  /**
   * Install pending version
   */
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    if ((*itr).op_ == OpType::INSERT) {
      continue;
    }
    Version *expected(nullptr), *ver, *pre_ver;
    for (;;) {
      if ((*itr).op_ == OpType::RMW || (*itr).op_ == OpType::DELETE || WRITE_LATEST_ONLY) {
        ver = expected = (*itr).rcdptr_->ldAcqLatest();
        if (this->wts_.ts_ < ver->ldAcqWts()) {
          result = false;
          goto FINISH_VALIDATION;
        }
      } else {
        if ((*itr).later_ver_) {
          pre_ver = (*itr).later_ver_;
          ver = pre_ver->ldAcqNext();
        } else {
          ver = expected = (*itr).rcdptr_->ldAcqLatest();
        }
        while (ver->ldAcqWts() > this->wts_.ts_) {
          (*itr).later_ver_ = ver;
          pre_ver = ver;
          ver = ver->ldAcqNext();
        }
      }

      if ((*itr).op_ == OpType::RMW || (*itr).op_ == OpType::DELETE
          || WRITE_LATEST_ONLY || ver == expected) {
        // Latter half of condition meanings that it was not traversaling
        // version list in not rmw mode.
        (*itr).new_ver_->strRelNext(expected);
        if ((*itr).rcdptr_->latest_.compare_exchange_strong(
                expected, (*itr).new_ver_, memory_order_acq_rel,
                memory_order_acquire)) {
          break;
        }
      } else {
        (*itr).new_ver_->strRelNext(ver);
        if (pre_ver->next_.compare_exchange_strong(ver, (*itr).new_ver_,
                                                   memory_order_acq_rel,
                                                   memory_order_acquire)) {
          break;
        }
      }
    }
    (*itr).finish_version_install_ = true;
  }

  /**
   * Read timestamp update
   */
  readTimestampUpdateInValidation();

  /**
   * version consistency check
   *(a) every previously visible version v of the records in the read set is the
   * currently visible version to the transaction.
   */
  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
    Version *ver;
    if ((*itr).later_ver_)
      ver = (*itr).later_ver_;
    else
      ver = (*itr).rcdptr_->ldAcqLatest();

    while (ver->ldAcqWts() >= this->wts_.ts_) ver = ver->ldAcqNext();
    // if write after read occured, it may happen "==".

    while (ver->ldAcqStatus() == VersionStatus::pending);
    while (ver->ldAcqStatus() != VersionStatus::committed
           && ver->ldAcqStatus() != VersionStatus::deleted) {
      ver = ver->ldAcqNext();
      while (ver->ldAcqStatus() == VersionStatus::pending);
    }
    /**
     * This part is different from the original.
     * Checking that view is not broken.
     * The original implementation is buggy because it does not have this.
     */
    if ((*itr).ver_ != ver) {
      result = false;
      goto FINISH_VALIDATION;
    }
  }

  /**
   * (b) every currently visible version v of the records in the write set
   * satisfies (v.rts) <= (tx.ts)
   */
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    if ((*itr).op_ == OpType::INSERT) {
      continue;
    }
    Version *ver = (*itr).new_ver_->ldAcqNext();
    while (ver->ldAcqStatus() == VersionStatus::pending);
    while (ver->ldAcqStatus() != VersionStatus::committed
           && ver->ldAcqStatus() != VersionStatus::deleted) {
      ver = ver->ldAcqNext();
      while (ver->ldAcqStatus() == VersionStatus::pending);
    }

    if (ver->ldAcqRts() > this->wts_.ts_ || ver->ldAcqStatus() == VersionStatus::deleted) {
      result = false;
      goto FINISH_VALIDATION;
    }
  }

  // validate the node set
  for (auto it : node_map_) {
    auto node = (MasstreeWrapper<Tuple>::node_type *) it.first;
    if (node->full_version_value() != it.second) {
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

void TxExecutor::swal() {
  if (!FLAGS_group_commit) {  // non-group commit
    SwalLock.w_lock();

    int i = 0;
    for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
      SLogSet[i] = (*itr).new_ver_;
      ++i;
    }

    double threshold = FLAGS_clocks_per_us * FLAGS_io_time_ns / 1000;
    uint64_t spinstart = rdtscp();
    while ((rdtscp() - spinstart) < threshold) {
    }  // spin-wait

    SwalLock.w_unlock();
  } else {  // group commit
    SwalLock.w_lock();
    for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
      SLogSet[GROUP_COMMIT_INDEX[0].obj_] = (*itr).new_ver_;
      ++GROUP_COMMIT_INDEX[0].obj_;
    }

    if (GROUP_COMMIT_COUNTER[0].obj_ == 0) {
      grpcmt_start_ = rdtscp();  // it can also initialize.
    }

    ++GROUP_COMMIT_COUNTER[0].obj_;

    if (GROUP_COMMIT_COUNTER[0].obj_ == FLAGS_group_commit) {
      double threshold = FLAGS_clocks_per_us * FLAGS_io_time_ns / 1000;
      uint64_t spinstart = rdtscp();
      while ((rdtscp() - spinstart) < threshold) {
      }  // spin-wait

      // group commit pending version.
      gcpv();
    }
    SwalLock.w_unlock();
  }
}

void TxExecutor::pwal() {
  if (!FLAGS_group_commit) {
    int i = 0;
    for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
      PLogSet[thid_][i] = (*itr).new_ver_;
      ++i;
    }

    // it gives lat ency instead of flush.
    double threshold = FLAGS_clocks_per_us * FLAGS_io_time_ns / 1000;
    uint64_t spinstart = rdtscp();
    while ((rdtscp() - spinstart) < threshold) {
    }  // spin-wait
  } else {
    for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
      PLogSet[thid_][GROUP_COMMIT_INDEX[thid_].obj_] = (*itr).new_ver_;
      ++GROUP_COMMIT_INDEX[this->thid_].obj_;
    }

    if (GROUP_COMMIT_COUNTER[this->thid_].obj_ == 0) {
      grpcmt_start_ = rdtscp();  // it can also initialize.
      ThreadRtsArrayForGroup[this->thid_].obj_ = this->rts_;
    }

    ++GROUP_COMMIT_COUNTER[this->thid_].obj_;

    if (GROUP_COMMIT_COUNTER[this->thid_].obj_ == FLAGS_group_commit) {
      // it gives latency instead of flush.
      double threshold = FLAGS_clocks_per_us * FLAGS_io_time_ns / 1000;
      uint64_t spinstart = rdtscp();
      while ((rdtscp() - spinstart) < threshold) {
      }  // spin-wait

      gcpv();
    }
  }
}

inline void TxExecutor::cpv()  // commit pending versions
{
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    /* memcpy は commit 確定前に書くことと，確定後に書くことができる．
     * 前提として，Cicada は OCC 性質を持つ．
     * 従って， commit 確定前に書く場合，read phase 区間が伸びることで，
     * updater に割り込まれやすくなって read validation failure 頻度が高くなり，
     * 性能劣化する性質と，逆に read validation failure 頻度が高くなることで
     * committed updater
     * が減少し，競合が減少して性能向上という複雑な二面性を持つ．
     * 確定後に書く気持ちは，read phase 区間をなるべく短くして，read validation
     * failure
     * を起こしにくくして性能向上させる．しかし，この場合は区間がオーバーラップする
     * tx
     * が増えるため競合増加して性能劣化するかもしれないという複雑な二面性を持つ．
     * 両方試してみた結果，特に変わらなかったため，確定後に書く．*/
    // we do not have to copy here anymore since we use objects created by worker
    // memcpy((*itr).new_ver_->val_, write_val_, VAL_SIZE);
#if SINGLE_EXEC
    (*itr).new_ver_->set(0, this->wts_.ts_);
#endif
    if ((*itr).op_ == OpType::DELETE) {
      Masstrees[get_storage((*itr).storage_)].remove_value((*itr).key_);
      (*itr).new_ver_->status_.store(VersionStatus::deleted, std::memory_order_release);
      gc_records_.push_back((*itr).rcdptr_);
    } else {
      (*itr).new_ver_->status_.store(VersionStatus::committed, std::memory_order_release);
    }
    gcq_.emplace_back(GCElement((*itr).storage_, (*itr).key_,
                                (*itr).rcdptr_, (*itr).new_ver_, this->wts_.ts_));
    ++(*itr).rcdptr_->continuing_commit_;
  }
}

void TxExecutor::gcpv() {
  if (FLAGS_s_wal) {
    for (unsigned int i = 0; i < GROUP_COMMIT_INDEX[0].obj_; ++i) {
      SLogSet[i]->status_.store(VersionStatus::committed, memory_order_release);
    }
    GROUP_COMMIT_COUNTER[0].obj_ = 0;
    GROUP_COMMIT_INDEX[0].obj_ = 0;
  } else if (FLAGS_p_wal) {
    for (unsigned int i = 0; i < GROUP_COMMIT_INDEX[thid_].obj_; ++i) {
      PLogSet[thid_][i]->status_.store(VersionStatus::committed,
                                       memory_order_release);
    }
    GROUP_COMMIT_COUNTER[thid_].obj_ = 0;
    GROUP_COMMIT_INDEX[thid_].obj_ = 0;
  }
}

/**
 * @brief function about abort.
 * clean-up local read/write set.
 * @return void
 */
void TxExecutor::abort() {
  // remove inserted records
  for (auto& we : write_set_) {
    if (we.op_ == OpType::INSERT) {
      Masstrees[get_storage(we.storage_)].remove_value(we.key_);
      delete we.rcdptr_;
    }
  }

  writeSetClean();
  read_set_.clear();
  node_map_.clear();

  if (FLAGS_group_commit) {
    chkGcpvTimeout();
  }

  this->wts_.set_clockBoost(FLAGS_clocks_per_us);

#if SINGLE_EXEC
#else
  /**
   * Maintenance phase
   */
  mainte();
#endif

#if BACK_OFF
  backoff();
#endif
}

// TODO: enable this if we want to use
// void TxExecutor::displayWriteSet() {
//   printf("displayWriteSet(): Th#%u: ", this->thid_);
//   for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
//     printf("%lu ", (*itr).key_);
//   }
//   cout << endl;
// }

bool TxExecutor::chkGcpvTimeout() {
  if (FLAGS_p_wal) {
    grpcmt_stop_ = rdtscp();
    if (chkClkSpan(grpcmt_start_, grpcmt_stop_,
                   FLAGS_group_commit_timeout_us * FLAGS_clocks_per_us)) {
      gcpv();
      return true;
    }
  } else if (FLAGS_s_wal) {
    grpcmt_stop_ = rdtscp();
    if (chkClkSpan(grpcmt_start_, grpcmt_stop_,
                   FLAGS_group_commit_timeout_us * FLAGS_clocks_per_us)) {
      SwalLock.w_lock();
      gcpv();
      SwalLock.w_unlock();
      return true;
    }
  }

  return false;
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
    Tuple* rec = gc_records_.front();
    Version* latest = rec->ldAcqLatest();
    if (latest->ldAcqWts() >= MinRts.load(memory_order_acquire)) break;
    if (latest->ldAcqStatus() != VersionStatus::deleted) ERR;
    delete rec;
    gc_records_.pop_front();
  }
}

void TxExecutor::mainte() {
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
  if (FLAGS_group_commit) {
    // check time out of commit pending versions
    chkGcpvTimeout();
  } else {
    cpv();
  }

  /* log write set & possibly group commit pending version
  if (FLAGS_p_wal) pwal();
  if (FLAGS_s_wal) swal();*/

  this->wts_.set_clockBoost(0);
  read_set_.clear();
  write_set_.clear();
  node_map_.clear();
#if ADD_ANALYSIS
  result_->local_commit_latency_ += rdtscp() - start;
#endif
}

bool TxExecutor::commit() {
  /**
   * Tanabe Optimization for analysis
   */
#if WORKER1_INSERT_DELAY_RPHASE
  if (unlikely(thid == 1) && WORKER1_INSERT_DELAY_RPHASE_US != 0) {
    clock_delay(WORKER1_INSERT_DELAY_RPHASE_US * FLAGS_clocks_per_us);
  }
#endif

  /**
   * Excerpt from original paper 3.1 Multi-Clocks Timestamp Allocation
   * A read-only transaction uses (thread.rts) instead, 
   * and does not track or validate the read set; 
   */
  if (this->is_ronly_) {
    read_set_.clear();
    node_map_.clear();
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
#if SINGLE_EXEC
#else
    mainte();
#endif
    return true;
  }
}

bool TxExecutor::isLeader() {
  return this->thid_ == 0;
}

void TxExecutor::leaderWork() {
  cicadaLeaderWork();
#if BACK_OFF
  leaderBackoffWork(backoff_, CicadaResult);
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
