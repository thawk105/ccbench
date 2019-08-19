
#include <stdio.h>
#include <string.h>  // memcpy
#include <sys/time.h>
#include <xmmintrin.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "include/common.hh"
#include "include/time_stamp.hh"
#include "include/transaction.hh"
#include "include/version.hh"

#include "../include/backoff.hh"
#include "../include/debug.hh"
#include "../include/masstree_wrapper.hh"
#include "../include/tsc.hh"

extern bool chkClkSpan(const uint64_t start, const uint64_t stop,
                       const uint64_t threshold);
extern void displaySLogSet();
extern void displayDB();

using namespace std;

#define MSK_TID 0b11111111

void TxExecutor::tbegin() {
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
  if (chkClkSpan(start, stop, 100 * CLOCKS_PER_US)) {
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

char *TxExecutor::tread(uint64_t key) {
#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif

  // read-own-writes
  // if n E write set
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    if ((*itr).key_ == key) {
#if ADD_ANALYSIS
      cres_->local_read_latency_ += rdtscp() - start;
#endif
      return write_val_;
      // in exp of tanabe, the new value was already decided for performance optimization.
    }
  }

  // if n E read set
  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
    if ((*itr).key_ == key) {
#if ADD_ANALYSIS
      cres_->local_read_latency_ += rdtscp() - start;
#endif
      return (*itr).ver_->val_;
    }
  }

#if MASSTREE_USE
  Tuple *tuple = MT.get_value(key);
#if ADD_ANALYSIS
  ++cres_->local_tree_traversal_;
#endif
#else
  Tuple *tuple = get_tuple(Table, key);
#endif

  // Search version
  Version *version;
#if SINGLE_EXEC
  version = &tuple->inline_version_;
#else
  uint64_t trts;
  if ((*this->pro_set_.begin()).ronly_) {
    trts = this->rts_;
  } else {
    trts = this->wts_.ts_;
  }

  version = tuple->ldAcqLatest();
  while (version->ldAcqWts() > trts)
    version = version->ldAcqNext();
  while (version->ldAcqStatus() != VersionStatus::committed) {
    while (version->ldAcqStatus() == VersionStatus::pending);
    if (version->ldAcqStatus() == VersionStatus::aborted)
      version = version->ldAcqNext();
  }
#endif

  // for fairness
  // ultimately, it is wasteful in prototype system.
  memcpy(return_val_, version->val_, VAL_SIZE);

  // if read-only, not track or validate read_set_
  if ((*this->pro_set_.begin()).ronly_ == false) {
    read_set_.emplace_back(key, tuple, version);
  }

#if ADD_ANALYSIS
  cres_->local_read_latency_ += rdtscp() - start;
#endif

#if INLINE_VERSION_PROMOTION
  if (version != &(tuple->inline_version_) &&
      MinRts.load(std::memory_order_acquire) > version->ldAcqWts() &&
      tuple->inline_version_.status_.load(std::memory_order_acquire) ==
          VersionStatus::unused) {
    twrite(key);
    if ((*this->pro_set_.begin()).ronly_) {
      (*this->pro_set_.begin()).ronly_ = false;
      read_set_.emplace_back(key, tuple, version);
    }
  }
#endif

  return version->val_;
}

void TxExecutor::twrite(uint64_t key) {
#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif
  // if n E write_set_
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    if ((*itr).key_ == key) {
#if ADD_ANALYSIS
      cres_->local_write_latency_ += rdtscp() - start;
#endif
      return;
      // tanabe の実験では，スレッドごとに新たに書く値が決まっている．
    }
  }

  bool rmw(false);
  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
    if ((*itr).key_ == key) {
      rmw = true;
      break;
    }
  }

#if MASSTREE_USE
  Tuple *tuple = MT.get_value(key);
#if ADD_ANALYSIS
  ++cres_->local_tree_traversal_;
#endif
#else
  Tuple *tuple = get_tuple(Table, key);
#endif

#if SINGLE_EXEC
  write_set_.emplace_back(key, tuple, &tuple->inline_version_, rmw);
#if ADD_ANALYSIS
  cres_->local_write_latency_ += rdtscp() - start;
#endif
#else

  Version *version = tuple->ldAcqLatest();
  while (version->ldAcqStatus() == VersionStatus::aborted)
    version = version->ldAcqNext();
  if (rmw) {
    // Search version (for early abort check)
    // search latest (committed or pending) version and use for early abort check
    // pending version may be committed, so use for early abort check.

    // early abort check(EAC)
    // here, version->status is commit or pending.
    if (version->wts_.load(memory_order_acquire) > this->wts_.ts_) {
      // if pending, it don't have to be aborted.
      // But it may be aborted, so early abort.
      // if committed, it must be aborted in validaiton phase due to order of
      // newest to oldest.
      this->status_ = TransactionStatus::abort;
#if ADD_ANALYSIS
      cres_->local_write_latency_ += rdtscp() - start;
#endif
      return;
    }
  } else {
    // not rmw
    while (version->wts_.load(memory_order_acquire) > this->wts_.ts_
        || version->ldAcqStatus() == VersionStatus::aborted)
      version = version->ldAcqNext();
  }

  if ((version->ldAcqRts() > this->wts_.ts_) &&
      (version->ldAcqStatus() == VersionStatus::committed)) {
    // it must be aborted in validation phase, so early abort.
    this->status_ = TransactionStatus::abort;
#if ADD_ANALYSIS
      cres_->local_write_latency_ += rdtscp() - start;
#endif
     return;
  }

#if INLINE_VERSION_OPT
  if (tuple->getInlineVersionRight()) {
    tuple->inline_version_.set(0, this->wts_.ts_);
    write_set_.emplace_back(key, tuple, &tuple->inline_version_, rmw);
    goto FINISH_TWRITE;
  }
#endif

  Version *newObject;
  if (reuse_version_from_gc_.empty()) {
    newObject = new Version(0, this->wts_.ts_);
#if ADD_ANALYSIS
    ++cres_->local_version_malloc_;
#endif
  } else {
    newObject = reuse_version_from_gc_.back();
    reuse_version_from_gc_.pop_back();
    newObject->set(0, this->wts_.ts_);
#if ADD_ANALYSIS
    ++cres_->local_version_reuse_;
#endif
  }
  write_set_.emplace_back(key, tuple, newObject, rmw);

#if INLINE_VERSION_OPT
FINISH_TWRITE:
#endif

#if ADD_ANALYSIS
  cres_->local_write_latency_ += rdtscp() - start;
#endif
#endif
  return;
}

bool TxExecutor::validation(const bool& quit) {
#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif
#if SINGLE_EXEC
#else
  if (continuing_commit_ < 5) {
    // Two optimizations can add unnecessary overhead under low contention
    // because they do not improve the performance of uncontended workloads.
    // Each thread adaptively omits both steps if the recent transactions have
    // been committed (5 in a row in our implementation).
    //
    // Sort write set by contention
    partial_sort(write_set_.begin(),
                 write_set_.begin() + (write_set_.size() / 2),
                 write_set_.end());

    // Pre-check version consistency
    // (b) every currently visible version v of the records in the write set
    // satisfies (v.rts) <= (tx.ts)
    for (auto itr = write_set_.begin();
         itr != write_set_.begin() + (write_set_.size() / 2); ++itr) {
      Version *version = (*itr).rcdptr_->ldAcqLatest();
      while (version->ldAcqStatus() != VersionStatus::committed)
        version = version->ldAcqNext();
      if ((*itr).rmw_ == false) {
        while (version->ldAcqWts() > this->wts_.ts_
            || version->ldAcqStatus() != VersionStatus::committed)
          version = version->ldAcqNext();
      }

      if (version->ldAcqRts() > this->wts_.ts_) {
#if ADD_ANALYSIS
        cres_->local_vali_latency_ += rdtscp() - start;
#endif
        return false;
      }
    }
  }

  // Install pending version
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    Version *expected, *version, *pre_version;
    for (;;) {
      version = expected = (*itr).rcdptr_->ldAcqLatest();

      if ((*itr).rmw_ == true) {
        if (version->ldAcqStatus() == VersionStatus::pending) {
          if (this->wts_.ts_ < version->ldAcqWts()) {
#if ADD_ANALYSIS
            cres_->local_vali_latency_ += rdtscp() - start;
#endif
            return false;
          }
          while (version->ldAcqStatus() == VersionStatus::pending)
            ;
        }

        while (version->ldAcqStatus() != VersionStatus::committed)
          version = version->ldAcqNext();
        // to avoid cascading aborts.
        // install pending version step blocks concurrent
        // transactions that share the same visible version
        // and have higher timestamp than (tx.ts).

        // to keep versions ordered by wts in the version list
        // if version is pending version, concurrent transaction that has lower
        // timestamp is aborted.
        if (this->wts_.ts_ < version->ldAcqWts()) {
#if ADD_ANALYSIS
          cres_->local_vali_latency_ += rdtscp() - start;
#endif
          return false;
        }
      } else {
        // not rmw
        while (version->ldAcqStatus() != VersionStatus::committed
            || version->ldAcqWts() > this->wts_.ts_) {
          pre_version = version;
          version = version->ldAcqNext();
        }
      }
        

      (*itr).newObject_->status_.store(VersionStatus::pending,
                                       memory_order_release);
      if ((*itr).rmw_ == true || version == expected) {
        // Latter half of condition meanings that it was not traversaling version list.
        (*itr).newObject_->strRelNext((*itr).rcdptr_->ldAcqLatest());
        if ((*itr).rcdptr_->latest_.compare_exchange_strong(
                expected, (*itr).newObject_, memory_order_acq_rel,
                memory_order_acquire))
          break;
      } else {
        (*itr).newObject_->strRelNext(version);
        if (pre_version->next_.compare_exchange_strong(
              version, (*itr).newObject_, memory_order_acq_rel,
              memory_order_acquire))
          break;
      }
    }
    (*itr).finish_version_install_ = true;
  }

  // Read timestamp update
  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
    uint64_t expected;
    expected = (*itr).ver_->ldAcqRts();
    for (;;) {
      if (expected > this->wts_.ts_) break;
      if ((*itr).ver_->rts_.compare_exchange_strong(expected, this->wts_.ts_,
                                                    memory_order_acq_rel,
                                                    memory_order_acquire))
        break;
    }
  }

  // version consistency check
  //(a) every previously visible version v of the records in the read set is the
  // currently visible version to the transaction.
  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
    Version *version, *init;
    version = init = (*itr).rcdptr_->ldAcqLatest();

    while (version->ldAcqWts() >= this->wts_.ts_)
      version = version->ldAcqNext();

    while (version->ldAcqStatus() != VersionStatus::committed) {
      while (version->ldAcqStatus() == VersionStatus::pending) {
        if (quit) {
          if (this->wts_.ts_ == version->wts_.load(memory_order_acquire)) printf("my version wait!\n");
        }
      }
      if (version->ldAcqStatus() == VersionStatus::committed) break;
      else version = version->ldAcqNext();
    }

    if ((*itr).ver_ == version) {
      break;
    } else {
#if ADD_ANALYSIS
      cres_->local_vali_latency_ += rdtscp() - start;
#endif
      return false;
    }
  }

  // (b) every currently visible version v of the records in the write set
  // satisfies (v.rts) <= (tx.ts)
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    Version *version = (*itr).newObject_->ldAcqNext();
    while (version->ldAcqStatus() != VersionStatus::committed)
      version = version->ldAcqNext();
    if (version->ldAcqRts() > this->wts_.ts_) {
#if ADD_ANALYSIS
      cres_->local_vali_latency_ += rdtscp() - start;
#endif
      return false;
    }
  }
#endif

#if ADD_ANALYSIS
  cres_->local_vali_latency_ += rdtscp() - start;
#endif
  return true;
}

void TxExecutor::swal() {
  if (!GROUP_COMMIT) {  // non-group commit
    SwalLock.w_lock();

    int i = 0;
    for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
      SLogSet[i] = (*itr).newObject_;
      ++i;
    }

    double threshold = CLOCKS_PER_US * IO_TIME_NS / 1000;
    uint64_t spinstart = rdtscp();
    while ((rdtscp() - spinstart) < threshold) {
    }  // spin-wait

    SwalLock.w_unlock();
  } else {  // group commit
    SwalLock.w_lock();
    for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
      SLogSet[GROUP_COMMIT_INDEX[0].obj_] = (*itr).newObject_;
      ++GROUP_COMMIT_INDEX[0].obj_;
    }

    if (GROUP_COMMIT_COUNTER[0].obj_ == 0) {
      grpcmt_start_ = rdtscp();  // it can also initialize.
    }

    ++GROUP_COMMIT_COUNTER[0].obj_;

    if (GROUP_COMMIT_COUNTER[0].obj_ == GROUP_COMMIT) {
      double threshold = CLOCKS_PER_US * IO_TIME_NS / 1000;
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
  if (!GROUP_COMMIT) {
    int i = 0;
    for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
      PLogSet[thid_][i] = (*itr).newObject_;
      ++i;
    }

    // it gives lat ency instead of flush.
    double threshold = CLOCKS_PER_US * IO_TIME_NS / 1000;
    uint64_t spinstart = rdtscp();
    while ((rdtscp() - spinstart) < threshold) {
    }  // spin-wait
  } else {
    for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
      PLogSet[thid_][GROUP_COMMIT_INDEX[thid_].obj_] = (*itr).newObject_;
      ++GROUP_COMMIT_INDEX[this->thid_].obj_;
    }

    if (GROUP_COMMIT_COUNTER[this->thid_].obj_ == 0) {
      grpcmt_start_ = rdtscp();  // it can also initialize.
      ThreadRtsArrayForGroup[this->thid_].obj_ = this->rts_;
    }

    ++GROUP_COMMIT_COUNTER[this->thid_].obj_;

    if (GROUP_COMMIT_COUNTER[this->thid_].obj_ == GROUP_COMMIT) {
      // it gives latency instead of flush.
      double threshold = CLOCKS_PER_US * IO_TIME_NS / 1000;
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
    memcpy((*itr).newObject_->val_, write_val_, VAL_SIZE);
#if SINGLE_EXEC
    (*itr).newObject_->set(0, this->wts_.ts_);
#else
    gcq_.push_back(GCElement((*itr).key_, (*itr).rcdptr_, (*itr).newObject_,
                             (*itr).newObject_->wts_));
#endif
    (*itr).newObject_->status_.store(VersionStatus::committed,
                                     std::memory_order_release);
  }
}

void TxExecutor::gcpv() {
  if (S_WAL) {
    for (unsigned int i = 0; i < GROUP_COMMIT_INDEX[0].obj_; ++i) {
      SLogSet[i]->status_.store(VersionStatus::committed, memory_order_release);
    }
    GROUP_COMMIT_COUNTER[0].obj_ = 0;
    GROUP_COMMIT_INDEX[0].obj_ = 0;
  } else if (P_WAL) {
    for (unsigned int i = 0; i < GROUP_COMMIT_INDEX[thid_].obj_; ++i) {
      PLogSet[thid_][i]->status_.store(VersionStatus::committed,
                                       memory_order_release);
    }
    GROUP_COMMIT_COUNTER[thid_].obj_ = 0;
    GROUP_COMMIT_INDEX[thid_].obj_ = 0;
  }
}

void TxExecutor::writeSetClean() {
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    if ((*itr).finish_version_install_)
      (*itr).newObject_->status_.store(VersionStatus::aborted,
                                       std::memory_order_release);
#if INLINE_VERSION_OPT
    else if ((*itr).newObject_ == &(*itr).rcdptr_->inline_version_)
      (*itr).rcdptr_->returnInlineVersionRight();
#endif
    else
      reuse_version_from_gc_.emplace_back((*itr).newObject_);
  }

  write_set_.clear();
}

void TxExecutor::earlyAbort() {
  writeSetClean();
  read_set_.clear();

  if (GROUP_COMMIT) {
    chkGcpvTimeout();
  }

  this->wts_.set_clockBoost(CLOCKS_PER_US);
  continuing_commit_ = 0;
  this->status_ = TransactionStatus::abort;
  ++cres_->local_abort_counts_;

#if USE_BACKOFF

#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif

  Backoff::backoff(CLOCKS_PER_US);

#if ADD_ANALYSIS
  cres_->local_backoff_latency_ += rdtscp() - start;
#endif

#endif
}

void TxExecutor::abort() {
  writeSetClean();
  read_set_.clear();

  if (GROUP_COMMIT) {
    chkGcpvTimeout();
  }

  this->wts_.set_clockBoost(CLOCKS_PER_US);
  continuing_commit_ = 0;
  ++cres_->local_abort_counts_;

#if USE_BACKOFF

#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif

  Backoff::backoff(CLOCKS_PER_US);

#if ADD_ANALYSIS
  cres_->local_backoff_latency_ += rdtscp() - start;
#endif

#endif
}

void TxExecutor::displayWset() {
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    printf("%lu ", (*itr).key_);
  }
  cout << endl;
}

bool TxExecutor::chkGcpvTimeout() {
  if (P_WAL) {
    grpcmt_stop_ = rdtscp();
    if (chkClkSpan(grpcmt_start_, grpcmt_stop_,
                   GROUP_COMMIT_TIMEOUT_US * CLOCKS_PER_US)) {
      gcpv();
      return true;
    }
  } else if (S_WAL) {
    grpcmt_stop_ = rdtscp();
    if (chkClkSpan(grpcmt_start_, grpcmt_stop_,
                   GROUP_COMMIT_TIMEOUT_US * CLOCKS_PER_US)) {
      SwalLock.w_lock();
      gcpv();
      SwalLock.w_unlock();
      return true;
    }
  }

  return false;
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
    ++cres_->local_gc_counts_;
#endif
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
        tuple->gClock_.store(0, memory_order_release);
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

      while (delTarget != nullptr) {
        // nextポインタ退避
        Version *tmp = delTarget->next_.load(std::memory_order_acquire);

#if INLINE_VERSION_OPT
        if (delTarget == &tuple->inline_version_)
          gcq_.front().rcdptr_->returnInlineVersionRight();
        else
          reuse_version_from_gc_.emplace_back(delTarget);
        goto FINISH_PUSH_BACK_REUSE_VERSION;
#endif

        reuse_version_from_gc_.emplace_back(delTarget);

#if INLINE_VERSION_OPT
      FINISH_PUSH_BACK_REUSE_VERSION:
#endif

#if ADD_ANALYSIS
        ++cres_->local_gc_version_counts_;
#endif

        delTarget = tmp;
      }

      // releases the lock
      gcq_.front().rcdptr_->returnGCRight();
      gcq_.pop_front();
    }

    __atomic_store_n(&(GCExecuteFlag[thid_].obj_), 0, __ATOMIC_RELEASE);
  }

  this->gcstop_ = rdtscp();
  if (chkClkSpan(this->gcstart_, this->gcstop_, GC_INTER_US * CLOCKS_PER_US)) {
    __atomic_store_n(&(GCFlag[thid_].obj_), 1, __ATOMIC_RELEASE);
    this->gcstart_ = this->gcstop_;
  }
  //-----
#if ADD_ANALYSIS
  cres_->local_gc_latency_ += rdtscp() - start;
#endif
}

void TxExecutor::writePhase() {
#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif
  if (GROUP_COMMIT) {
    // check time out of commit pending versions
    chkGcpvTimeout();
  } else {
    cpv();
  }

  // log write set & possibly group commit pending version
  if (P_WAL) pwal();
  if (S_WAL) swal();

  //ここまで来たらコミットしている
  this->wts_.set_clockBoost(0);
  read_set_.clear();
  write_set_.clear();
  ++continuing_commit_;
  ++cres_->local_commit_counts_;
#if ADD_ANALYSIS
  cres_->local_commit_latency_ += rdtscp() - start;
#endif
}
