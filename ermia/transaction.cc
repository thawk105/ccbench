
#include <string.h>
#include <algorithm>
#include <atomic>
#include <bitset>

#include "../include/atomic_wrapper.hh"
#include "../include/backoff.hh"
#include "../include/debug.hh"
#include "../include/masstree_wrapper.hh"
#include "../include/tsc.hh"
#include "include/common.hh"
#include "include/transaction.hh"
#include "include/version.hh"

extern bool chkClkSpan(const uint64_t start, const uint64_t stop,
                       const uint64_t threshold);

using namespace std;

/**
 * @brief Search xxx set
 * @detail Search element of local set corresponding to given key.
 * In this prototype system, the value to be updated for each worker thread 
 * is fixed for high performance, so it is only necessary to check the key match.
 * @param Key [in] the key of key-value
 * @return Corresponding element of local set
 */
inline SetElement<Tuple> *TxExecutor::searchReadSet(unsigned int key) {
  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
    if ((*itr).key_ == key) return &(*itr);
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
inline SetElement<Tuple> *TxExecutor::searchWriteSet(unsigned int key) {
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    if ((*itr).key_ == key) return &(*itr);
  }

  return nullptr;
}

/**
 * @brief Initialize function of transaction.
 * Allocate timestamp.
 * @return void
 */
void TxExecutor::tbegin() {
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
                                      TransactionStatus::inFlight);
#if ADD_ANALYSIS
    ++eres_->local_TMT_element_malloc_;
#endif
  } else {
    /**
     * If it has cache, this transaction use it.
     */
    newElement = gcobject_.reuse_TMT_element_from_gc_.back();
    gcobject_.reuse_TMT_element_from_gc_.pop_back();
    newElement->set(0, 0, UINT32_MAX, lastcstamp, TransactionStatus::inFlight);
#if ADD_ANALYSIS
    ++eres_->local_TMT_element_reuse_;
#endif
  }

  /**
   * Check the latest commit timestamp
   */
  for (unsigned int i = 0; i < FLAGS_thread_num; ++i) {
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
  status_ = TransactionStatus::inFlight;
}

/**
 * @brief Transaction read function.
 * @param [in] key The key of key-value
 */
void TxExecutor::ssn_tread(uint64_t key) {
#if ADD_ANALYSIS
  uint64_t start(rdtscp());
#endif

  /**
   * read-own-writes, re-read from previous read in the same tx.
   */
  if (searchWriteSet(key) || searchReadSet(key)) goto FINISH_TREAD;

  /**
   * Search versions from data structure.
   */
  Tuple *tuple;
#if MASSTREE_USE
  tuple = MT.get_value(key);
#if ADD_ANALYSIS
  ++eres_->local_tree_traversal_;
#endif
#else
  tuple = get_tuple(Table, key);
#endif

  /**
   * Move to the points of this view.
   */
  Version *ver;
  ver = tuple->latest_.load(memory_order_acquire);
  while (ver->status_.load(memory_order_acquire) != VersionStatus::committed ||
         txid_ < ver->cstamp_.load(memory_order_acquire))
    ver = ver->prev_;

  if (ver->psstamp_.atomicLoadSstamp() == (UINT32_MAX & ~(TIDFLAG))) {
    // no overwrite yet
    read_set_.emplace_back(key, tuple, ver);
  } else {
    // update pi with r:w edge
    this->sstamp_ =
            min(this->sstamp_, (ver->psstamp_.atomicLoadSstamp() >> TIDFLAG));
  }
  upReadersBits(ver);

  verify_exclusion_or_abort();
  if (this->status_ == TransactionStatus::aborted) {
    goto FINISH_TREAD;
  }

  /**
   * read payload.
   */
  memcpy(this->return_val_, ver->val_, VAL_SIZE);
#if ADD_ANALYSIS
  ++eres_->local_memcpys;
#endif

FINISH_TREAD:
#if ADD_ANALYSIS
  eres_->local_read_latency_ += rdtscp() - start;
#endif
  return;
}

/**
 * @brief Transaction write function.
 * @param [in] key The key of key-value
 */
void TxExecutor::ssn_twrite(uint64_t key) {
#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif

  /**
   * update local write set.
   */
  if (searchWriteSet(key)) goto FINISH_TWRITE;

  /**
   * avoid false positive.
   */
  Tuple *tuple;
  tuple = nullptr;
  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
    if ((*itr).key_ == key) {
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
#if MASSTREE_USE
    tuple = MT.get_value(key);
#if ADD_ANALYSIS
    ++eres_->local_tree_traversal_;
#endif
#else
    tuple = get_tuple(Table, key);
#endif
  }

  /**
   * If v not in t.writes:
   * first-updater-wins rule
   * Forbid a transaction to update  a record that has a committed head version
   * later than its begin timestamp.
   */
  Version *expected, *desired;
  if (gcobject_.reuse_version_from_gc_.empty()) {
    desired = new Version();
#if ADD_ANALYSIS
    ++eres_->local_version_malloc_;
#endif
  } else {
    desired = gcobject_.reuse_version_from_gc_.back();
    gcobject_.reuse_version_from_gc_.pop_back();
    desired->init();
#if ADD_ANALYSIS
    ++eres_->local_version_reuse_;
#endif
  }
  desired->cstamp_.store(
          this->txid_,
          memory_order_relaxed);  // read operation, write operation,
  // it is also accessed by garbage collection.

  Version *vertmp;
  expected = tuple->latest_.load(memory_order_acquire);
  for (;;) {
    // w-w conflict
    // first updater wins rule
    if (expected->status_.load(memory_order_acquire) ==
        VersionStatus::inFlight) {
      if (this->txid_ <= expected->cstamp_.load(memory_order_acquire)) {
        this->status_ = TransactionStatus::aborted;
        TMT[thid_]->status_.store(TransactionStatus::aborted,
                                  memory_order_release);
        gcobject_.reuse_version_from_gc_.emplace_back(desired);
        goto FINISH_TWRITE;
      }

      expected = tuple->latest_.load(memory_order_acquire);
      continue;
    }

    // if latest version is not comitted.
    vertmp = expected;
    while (vertmp->status_.load(memory_order_acquire) !=
           VersionStatus::committed)
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
      goto FINISH_TWRITE;
    }

    desired->prev_ = expected;
    if (tuple->latest_.compare_exchange_strong(
            expected, desired, memory_order_acq_rel, memory_order_acquire))
      break;
  }

  /**
   * Insert my tid for ver->prev_->sstamp_ 
   */
  uint64_t tmpTID;
  tmpTID = thid_;
  tmpTID = tmpTID << 1;
  tmpTID |= 1;
  desired->prev_->psstamp_.atomicStoreSstamp(tmpTID);

  /**
   * Update eta with w:r edge
   */
  this->pstamp_ =
          max(this->pstamp_, desired->prev_->psstamp_.atomicLoadPstamp());
  write_set_.emplace_back(key, tuple, desired);

  verify_exclusion_or_abort();

FINISH_TWRITE:
#if ADD_ANALYSIS
  eres_->local_write_latency_ += rdtscp() - start;
#endif
  return;
}

/**
 * Serial validation (SSN)
 */
void TxExecutor::ssn_commit() {
  this->status_ = TransactionStatus::committing;
  TransactionTable *tmt = loadAcquire(TMT[thid_]);
  tmt->status_.store(TransactionStatus::committing);

  this->cstamp_ = ++Lsn;
  tmt->cstamp_.store(this->cstamp_, memory_order_release);

  // begin pre-commit
  SsnLock.lock();

  // finalize eta(T)
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    pstamp_ = max(pstamp_, (*itr).ver_->prev_->psstamp_.atomicLoadPstamp());
  }

  // finalize pi(T)
  sstamp_ = min(sstamp_, cstamp_);
  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
    uint32_t verSstamp = (*itr).ver_->psstamp_.atomicLoadSstamp();
    // if the lowest bit raise, the record was overwrited by other concurrent
    // transactions. but in serial SSN, the validation of the concurrent
    // transactions will be done after that of this transaction. So it can skip.
    // And if the lowest bit raise, the stamp is TID;
    if (verSstamp & 1) continue;
    sstamp_ = min(sstamp_, verSstamp >> 1);
  }

  // ssn_check_exclusion
  if (pstamp_ < sstamp_)
    status_ = TransactionStatus::committed;
  else {
    status_ = TransactionStatus::aborted;
    SsnLock.unlock();
    return;
  }

  // update eta
  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
    (*itr).ver_->psstamp_.atomicStorePstamp(
            max((*itr).ver_->psstamp_.atomicLoadPstamp(), cstamp_));
    // down readers bit
    downReadersBits((*itr).ver_);
  }

  // update pi
  uint64_t verSstamp = this->sstamp_;
  verSstamp = verSstamp << 1;
  verSstamp &= ~(1);

  uint64_t verCstamp = cstamp_;
  verCstamp = verCstamp << 1;
  verCstamp &= ~(1);

  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    (*itr).ver_->prev_->psstamp_.atomicStoreSstamp(verSstamp);
    // initialize new version
    (*itr).ver_->psstamp_.atomicStorePstamp(cstamp_);
    (*itr).ver_->cstamp_ = verCstamp;
  }

  // logging
  //?*

  // status, inFlight -> committed
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    memcpy((*itr).ver_->val_, write_val_, VAL_SIZE);
#if ADD_ANALYSIS
    ++eres_->local_memcpys;
#endif
    (*itr).ver_->status_.store(VersionStatus::committed, memory_order_release);
  }

  this->status_ = TransactionStatus::committed;
  SsnLock.unlock();
  read_set_.clear();
  write_set_.clear();
  return;
}

/**
 * Parallel validation (latch-free SSN)
 */
void TxExecutor::ssn_parallel_commit() {
#if ADD_ANALYSIS
  uint64_t start(rdtscp());
#endif
  this->status_ = TransactionStatus::committing;
  TransactionTable *tmt = TMT[thid_];
  tmt->status_.store(TransactionStatus::committing);

  this->cstamp_ = ++Lsn;
  // this->cstamp_ = 2; test for effect of centralized counter in YCSB-C (read
  // only workload)

  tmt->cstamp_.store(this->cstamp_, memory_order_release);

  // begin pre-commit

  // finalize pi(T)
  this->sstamp_ = min(this->sstamp_, this->cstamp_);
  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
    uint32_t v_sstamp = (*itr).ver_->psstamp_.atomicLoadSstamp();
    // if lowest bits raise, it is TID
    if (v_sstamp & TIDFLAG) {
      // identify worker by using TID
      uint8_t worker = (v_sstamp >> TIDFLAG);
      /**
       * If worker is Inflight state, it will be committed with newer timestamp than this.
       * Then, the worker can't be pi, so skip.
       * If not, check.
       */
      tmt = loadAcquire(TMT[worker]);
      if (tmt->status_.load(memory_order_acquire) ==
          TransactionStatus::committing) {
        /**
         * Worker is in ssn_parallel_commit().
         * So it wait worker to get cstamp.
         */
        while (tmt->cstamp_.load(memory_order_acquire) == 0);
        /**
         * If worker->cstamp_ is less than this->cstamp_, the worker can be pi.
         */
        if (tmt->cstamp_.load(memory_order_acquire) < this->cstamp_) {
          /**
           * It wait worker to end parallel_commit (determine sstamp).
           */
          while (tmt->status_.load(memory_order_acquire) ==
                 TransactionStatus::committing);
          if (tmt->status_.load(memory_order_acquire) ==
              TransactionStatus::committed) {
            this->sstamp_ =
                    min(this->sstamp_, tmt->sstamp_.load(memory_order_acquire));
          }
        }
      }
    } else {
      this->sstamp_ = min(this->sstamp_, v_sstamp >> TIDFLAG);
    }
  }

  /**
   * finalize eta.
   */
  uint64_t one = 1;
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    /**
     * for r in v.prev.readers
     */
    Version *ver = (*itr).ver_;
    while (ver->status_.load(memory_order_acquire) != VersionStatus::committed)
      ver = ver->prev_;
    uint64_t rdrs = ver->readers_.load(memory_order_acquire);
    for (unsigned int worker = 0; worker < FLAGS_thread_num; ++worker) {
      if ((rdrs & (one << worker)) ? 1 : 0) {
        tmt = loadAcquire(TMT[worker]);
        /**
         * It can ignore if the reader is committing.
         * It can ignore if the reader is inFlight because
         * the reader will get larger cstamp and it can't be eta of this.
         */
        if (tmt->status_.load(memory_order_acquire) ==
            TransactionStatus::committing) {
          while (tmt->cstamp_.load(memory_order_acquire) == 0);
          /**
           * If worker->cstamp_ is less than this->cstamp_, it can be eta.
           */
          if (tmt->cstamp_.load(memory_order_acquire) < this->cstamp_) {
            /**
             * Wait end of parallel_commit (determing sstamp).
             */
            while (tmt->status_.load(memory_order_acquire) ==
                   TransactionStatus::committing);
            if (tmt->status_.load(memory_order_acquire) ==
                TransactionStatus::committed) {
              this->pstamp_ =
                      max(this->pstamp_, tmt->cstamp_.load(memory_order_acquire));
            }
          }
        }
      }
    }
    /**
     * Re-read pstamp in case we missed any reader
     */
    this->pstamp_ = max(this->pstamp_, ver->psstamp_.atomicLoadPstamp());
  }

  tmt = TMT[thid_];
  /**
   * ssn_check_exclusion
   */
  if (pstamp_ < sstamp_) {
    status_ = TransactionStatus::committed;
    tmt->sstamp_.store(this->sstamp_, memory_order_release);
    tmt->status_.store(TransactionStatus::committed, memory_order_release);
  } else {
    status_ = TransactionStatus::aborted;
    tmt->status_.store(TransactionStatus::aborted, memory_order_release);
    goto FINISH_PARALLEL_COMMIT;
  }

#if ADD_ANALYSIS
  eres_->local_vali_latency_ += rdtscp() - start;
  start = rdtscp();
#endif

  /**
   * update eta.
   */
  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
    Psstamp pstmp;
    pstmp.obj_ = (*itr).ver_->psstamp_.atomicLoad();
    while (pstmp.pstamp_ < this->cstamp_) {
      if ((*itr).ver_->psstamp_.atomicCASPstamp(pstmp.pstamp_, this->cstamp_))
        break;
      pstmp.obj_ = (*itr).ver_->psstamp_.atomicLoad();
    }
    downReadersBits((*itr).ver_);
  }

  /**
   * update pi.
   */
  uint64_t verSstamp;
  verSstamp = this->sstamp_ << TIDFLAG;
  verSstamp &= ~(TIDFLAG);

  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    Version *next_committed = (*itr).ver_->prev_;
    while (next_committed->status_.load(memory_order_acquire) !=
           VersionStatus::committed)
      next_committed = next_committed->prev_;
    next_committed->psstamp_.atomicStoreSstamp(verSstamp);
    (*itr).ver_->cstamp_.store(this->cstamp_, memory_order_release);
    (*itr).ver_->psstamp_.atomicStorePstamp(this->cstamp_);
    (*itr).ver_->psstamp_.atomicStoreSstamp(UINT32_MAX & ~(TIDFLAG));
    memcpy((*itr).ver_->val_, write_val_, VAL_SIZE);
#if ADD_ANALYSIS
    ++eres_->local_memcpys;
#endif
    (*itr).ver_->status_.store(VersionStatus::committed, memory_order_release);
    gcobject_.gcq_for_version_.emplace_back(
            GCElement((*itr).key_, (*itr).rcdptr_, (*itr).ver_, this->cstamp_));
  }

  // logging
  //?*

  read_set_.clear();
  write_set_.clear();
  TMT[thid_]->lastcstamp_.store(cstamp_, memory_order_release);

FINISH_PARALLEL_COMMIT:
#if ADD_ANALYSIS
  eres_->local_commit_latency_ += rdtscp() - start;
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
    Version *next_committed = (*itr).ver_->prev_;
    while (next_committed->status_.load(memory_order_acquire) !=
           VersionStatus::committed)
      next_committed = next_committed->prev_;
    /**
     * cancel successor mark(sstamp).
     */
    next_committed->psstamp_.atomicStoreSstamp(UINT32_MAX & ~(TIDFLAG));
    (*itr).ver_->status_.store(VersionStatus::aborted, memory_order_release);
  }
  write_set_.clear();

  /**
   * notify that this transaction finishes reading the version now.
   */
  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr)
    downReadersBits((*itr).ver_);

  read_set_.clear();
  ++eres_->local_abort_counts_;

#if BACK_OFF

#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif

  Backoff::backoff(FLAGS_clocks_per_us);

#if ADD_ANALYSIS
  eres_->local_backoff_latency_ += rdtscp() - start;
#endif

#endif
}

void TxExecutor::verify_exclusion_or_abort() {
  if (this->pstamp_ >= this->sstamp_) {
    this->status_ = TransactionStatus::aborted;
    TransactionTable *tmt = loadAcquire(TMT[thid_]);
    tmt->status_.store(TransactionStatus::aborted, memory_order_release);
  }
}

void TxExecutor::mainte() {
  gcstop_ = rdtscp();
  if (chkClkSpan(gcstart_, gcstop_, FLAGS_gc_inter_us * FLAGS_clocks_per_us)) {
    uint32_t loadThreshold = gcobject_.getGcThreshold();
    if (pre_gc_threshold_ != loadThreshold) {
#if ADD_ANALYSIS
      uint64_t start;
      start = rdtscp();
      ++eres_->local_gc_counts_;
#endif
      gcobject_.gcTMTelement(eres_);
      gcobject_.gcVersion(eres_);
      pre_gc_threshold_ = loadThreshold;
      gcstart_ = gcstop_;
#if ADD_ANALYSIS
      eres_->local_gc_latency_ += rdtscp() - start;
#endif
    }
  }
}

void TxExecutor::dispWS() {
  cout << "th " << this->thid_ << " : write set : ";
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    cout << "(" << (*itr).key_ << ", " << (*itr).ver_->val_ << "), ";
  }
  cout << endl;
}

void TxExecutor::dispRS() {
  cout << "th " << this->thid_ << " : read set : ";
  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
    cout << "(" << (*itr).key_ << ", " << (*itr).ver_->val_ << "), ";
  }
  cout << endl;
}
