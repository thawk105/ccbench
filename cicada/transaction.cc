
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

void TxExecutor::tread(const uint64_t key) {
#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif  // if ADD_ANALYSIS

  // read-own-writes
  // if n E write set
  if (searchWriteSet(key) || searchReadSet(key)) goto FINISH_TREAD;

#if MASSTREE_USE
  Tuple *tuple;
  tuple = MT.get_value(key);

#if ADD_ANALYSIS
  ++cres_->local_tree_traversal_;
#endif  // if ADD_ANALYSIS

#else
  Tuple *tuple = get_tuple(Table, key);
#endif  // if MASSTREE_USE

  // Search version
  Version *ver, *later_ver;
  later_ver = nullptr;

#if SINGLE_EXEC
  ver = &tuple->inline_ver_;
#else
  uint64_t trts;
  if ((*this->pro_set_.begin()).ronly_) {
    trts = this->rts_;
  } else {
    trts = this->wts_.ts_;
  }

  ver = tuple->ldAcqLatest();
  while (ver->ldAcqWts() > trts) {
    later_ver = ver;
    ver = ver->ldAcqNext();
  }
  while (ver->status_.load(memory_order_acquire) != VersionStatus::committed) {
    while (ver->status_.load(memory_order_acquire) == VersionStatus::pending) {
    }
    if (ver->status_.load(memory_order_acquire) == VersionStatus::aborted) {
      ver = ver->ldAcqNext();
    }
  }
#endif

  // for fairness
  // ultimately, it is wasteful in prototype system.
  memcpy(return_val_, ver->val_, VAL_SIZE);

  // if read-only, not track or validate read_set_
  if ((*this->pro_set_.begin()).ronly_ == false) {
    read_set_.emplace_back(key, tuple, later_ver, ver);
  }

#if INLINE_VERSION_OPT
#if INLINE_VERSION_PROMOTION
#if ADD_ANALYSIS
  cres_->local_read_latency_ += rdtscp() - start;
#endif  // if ADD_ANALYSIS
  inlineVersionPromotion(key, tuple, later_ver, ver);
  goto END_TREAD;
#endif  // if INLINE_VERSION_PROMOTION
#endif  // if INLINE_VERSION_OPT

FINISH_TREAD:

#if ADD_ANALYSIS
  cres_->local_read_latency_ += rdtscp() - start;
#endif

END_TREAD:

  return;
}

void TxExecutor::twrite(const uint64_t key) {
#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif  // if ADD_ANALYSIS

  if (searchWriteSet(key)) goto FINISH_TWRITE;

  Tuple *tuple;
  bool rmw;
  rmw = false;
  ReadElement<Tuple> *re;
  re = searchReadSet(key);
  if (re) {
    rmw = true;
    tuple = re->rcdptr_;
    /* 現時点で，シンプルな改造で re->later_ver_ 
     * 情報を受け取って利用することは難しい．
     * 何故なら，read only tx で発生した inline version promotion による
     * write であれば，read-only 専用のタイムスタンプによるサーチにおいて
     * later_ver であるが，write の later_ver には不適格．
     * ここを考慮してゴニョゴニョしても，利益が小さくて性能が上がらなかった．*/
    /* Now, it is difficult to use re->later_ver_ by simple customize.
     * Because if this write is from inline version promotion which is from read only tx,
     * the re->later_ver is suitable for read only search by read only timestamp.
     * Of course, it is unsuitable for search for write.
     * It is high cost to consider these things.
     * I try many somethings but it can't improve performance. cost > profit.*/
  } else {
#if MASSTREE_USE
    tuple = MT.get_value(key);

#if ADD_ANALYSIS
    ++cres_->local_tree_traversal_;
#endif  // if ADD_ANALYSIS

#else
    tuple = get_tuple(Table, key);
#endif  // if MASSTREE_USE
  }

#if SINGLE_EXEC
  write_set_.emplace_back(key, tuple, nullptr, &tuple->inline_ver_, rmw);
#else
  Version *later_ver, *ver;
  later_ver = nullptr;
  ver = tuple->ldAcqLatest();

  if (rmw || WRITE_LATEST_ONLY) {
    // Search version (for early abort check)
    // search latest (committed or pending) version and use for early abort
    // check pending version may be committed, so use for early abort check.

    // early abort check(EAC)
    // here, version->status is commit or pending.
    if (ver->wts_.load(memory_order_acquire) > this->wts_.ts_) {
      // if pending, it don't have to be aborted.
      // But it may be aborted, so early abort.
      // if committed, it must be aborted in validaiton phase due to order of
      // newest to oldest.
      this->status_ = TransactionStatus::abort;
      goto FINISH_TWRITE;
    }
  } else {
    // not rmw
    while (ver->wts_.load(memory_order_acquire) > this->wts_.ts_) {
      later_ver = ver;
      ver = ver->ldAcqNext();
    }
  }

  if ((ver->ldAcqRts() > this->wts_.ts_) &&
      (ver->ldAcqStatus() == VersionStatus::committed)) {
    // it must be aborted in validation phase, so early abort.
    this->status_ = TransactionStatus::abort;
    goto FINISH_TWRITE;
  }

  Version *new_ver;
  new_ver = newVersionGeneration(tuple);
  write_set_.emplace_back(key, tuple, later_ver, new_ver, rmw);
#endif  // if SINGLE_EXEC

FINISH_TWRITE:

#if ADD_ANALYSIS
  cres_->local_write_latency_ += rdtscp() - start;
#endif  // if ADD_ANALYSIS

  return;
}

bool TxExecutor::validation() {
#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif  // if ADD_ANALYSIS

  bool result(true);
  if (!precheckInValidation()) {
    result = false;
    goto FINISH_VALIDATION;
  }

  // Install pending version
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    Version *expected(nullptr), *ver, *pre_ver;
    for (;;) {
      if ((*itr).rmw_ || WRITE_LATEST_ONLY) {
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

      if ((*itr).rmw_ == true || WRITE_LATEST_ONLY || ver == expected) {
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

  // Read timestamp update
  readTimestampUpdateInValidation();

  // version consistency check
  //(a) every previously visible version v of the records in the read set is the
  // currently visible version to the transaction.
  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
    Version *ver;
    if ((*itr).later_ver_)
      ver = (*itr).later_ver_;
    else
      ver = (*itr).rcdptr_->ldAcqLatest();

    while (ver->ldAcqWts() >= this->wts_.ts_) ver = ver->ldAcqNext();
    // if write after read occured, it may happen "==".

    while (ver->ldAcqStatus() == VersionStatus::pending)
      ;
    while (ver->ldAcqStatus() != VersionStatus::committed) {
      ver = ver->ldAcqNext();
      while (ver->ldAcqStatus() == VersionStatus::pending)
        ;
    }
    /* この部分は，オリジナル手法と異なる．
     * 現在 view となるバージョンが，read operation 時の view と同一かをチェックしている．
     * オリジナル手法はこれがないため，この実装より性能は良いが view が壊れている.*/
    if ((*itr).ver_ != ver) {
      result = false;
      goto FINISH_VALIDATION;
    }
  }

  // (b) every currently visible version v of the records in the write set
  // satisfies (v.rts) <= (tx.ts)
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    Version *ver = (*itr).new_ver_->ldAcqNext();
    while (ver->ldAcqStatus() == VersionStatus::pending)
      ;
    while (ver->ldAcqStatus() != VersionStatus::committed) {
      ver = ver->ldAcqNext();
      while (ver->ldAcqStatus() == VersionStatus::pending)
        ;
    }

    if (ver->ldAcqRts() > this->wts_.ts_) {
      result = false;
      goto FINISH_VALIDATION;
    }
  }

FINISH_VALIDATION:
#if ADD_ANALYSIS
  cres_->local_vali_latency_ += rdtscp() - start;
#endif  // if ADD_ANALYSIS
  return result;
}

void TxExecutor::swal() {
  if (!GROUP_COMMIT) {  // non-group commit
    SwalLock.w_lock();

    int i = 0;
    for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
      SLogSet[i] = (*itr).new_ver_;
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
      SLogSet[GROUP_COMMIT_INDEX[0].obj_] = (*itr).new_ver_;
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
      PLogSet[thid_][i] = (*itr).new_ver_;
      ++i;
    }

    // it gives lat ency instead of flush.
    double threshold = CLOCKS_PER_US * IO_TIME_NS / 1000;
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
    /* memcpy は commit 確定前に書くことと，確定後に書くことができる．
     * 前提として，Cicada は OCC 性質を持つ．
     * 従って， commit 確定前に書く場合，read phase 区間が伸びることで，
     * updater に割り込まれやすくなって read validation failure 頻度が高くなり，
     * 性能劣化する性質と，逆に read validation failure 頻度が高くなることで
     * committed updater が減少し，競合が減少して性能向上という複雑な二面性を持つ．
     * 確定後に書く気持ちは，read phase 区間をなるべく短くして，read validation failure 
     * を起こしにくくして性能向上させる．しかし，この場合は区間がオーバーラップする
     * tx が増えるため競合増加して性能劣化するかもしれないという複雑な二面性を持つ．
     * 両方試してみた結果，特に変わらなかったため，確定後に書く．*/
    memcpy((*itr).new_ver_->val_, write_val_, VAL_SIZE);
#if SINGLE_EXEC
    (*itr).new_ver_->set(0, this->wts_.ts_);
#endif
    (*itr).new_ver_->status_.store(VersionStatus::committed,
                                   std::memory_order_release);
    gcq_.emplace_back(GCElement((*itr).key_, (*itr).rcdptr_, (*itr).new_ver_,
                                this->wts_.ts_));
    ++(*itr).rcdptr_->continuing_commit_;
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

void TxExecutor::earlyAbort() {
  writeSetClean();
  read_set_.clear();

  if (GROUP_COMMIT) {
    chkGcpvTimeout();
  }

  this->wts_.set_clockBoost(CLOCKS_PER_US);
  this->status_ = TransactionStatus::abort;
  ++cres_->local_abort_counts_;

#if BACK_OFF
  backoff();
#endif
}

void TxExecutor::abort() {
  writeSetClean();
  read_set_.clear();

  if (GROUP_COMMIT) {
    chkGcpvTimeout();
  }

  this->wts_.set_clockBoost(CLOCKS_PER_US);
  ++cres_->local_abort_counts_;

#if BACK_OFF
  backoff();
#endif
}

void TxExecutor::displayWriteSet() {
  printf("displayWriteSet(): Th#%u: ", this->thid_);
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

  /* log write set & possibly group commit pending version
  if (P_WAL) pwal();
  if (S_WAL) swal();*/

  this->wts_.set_clockBoost(0);
  read_set_.clear();
  write_set_.clear();
  ++cres_->local_commit_counts_;
#if ADD_ANALYSIS
  cres_->local_commit_latency_ += rdtscp() - start;
#endif
}
