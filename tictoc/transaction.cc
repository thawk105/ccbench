
#include <stdio.h>
#include <sys/time.h>

#include <algorithm>
#include <bitset>
#include <fstream>
#include <sstream>
#include <string>

#include "../include/atomic_wrapper.hh"
#include "../include/backoff.hh"
#include "../include/debug.hh"
#include "../include/inline.hh"
#include "../include/masstree_wrapper.hh"
#include "../include/tsc.hh"
#include "include/common.hh"
#include "include/transaction.hh"
#include "include/tuple.hh"

using namespace std;

TxExecutor::TxExecutor(int thid, Result *tres) : thid_(thid), tres_(tres) {
  read_set_.reserve(FLAGS_max_ope);
  write_set_.reserve(FLAGS_max_ope);
  pro_set_.reserve(FLAGS_max_ope);

  genStringRepeatedNumber(write_val_, VAL_SIZE, thid);
}

SetElement<Tuple> *TxExecutor::searchWriteSet(uint64_t key) {
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    if ((*itr).key_ == key) return &(*itr);
  }

  return nullptr;
}

SetElement<Tuple> *TxExecutor::searchReadSet(uint64_t key) {
  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
    if ((*itr).key_ == key) return &(*itr);
  }

  return nullptr;
}

void TxExecutor::begin() {
  this->status_ = TransactionStatus::inFlight;
  this->commit_ts_ = 0;
  this->appro_commit_ts_ = 0;
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
    ++tres_->local_preemptive_aborts_counts_;
#endif
    return true;
  }

  return false;
}

void TxExecutor::read(uint64_t key) {
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

  /**
   * read-own-writes or re-read from local read set.
   */
  if (searchReadSet(key) || searchWriteSet(key)) goto FINISH_READ;

  /**
   * Search tuple from data structure.
   */
  Tuple *tuple;
#if MASSTREE_USE
  tuple = MT.get_value(key);
#if ADD_ANALYSIS
  ++tres_->local_tree_traversal_;
#endif
#else
  tuple = get_tuple(Table, key);
#endif

  v1.obj_ = __atomic_load_n(&(tuple->tsw_.obj_), __ATOMIC_ACQUIRE);
  for (;;) {
    if (v1.lock) {
      /**
       * Check whether it can be serialized between old points and new points.
       */
#if PREEMPTIVE_ABORTS
      if (preemptiveAborts(v1)) goto FINISH_READ;
#endif
      v1.obj_ = __atomic_load_n(&(tuple->tsw_.obj_), __ATOMIC_ACQUIRE);
      continue;
    }

    /**
     * read payload.
     */
    memcpy(return_val_, tuple->val_, VAL_SIZE);

    v2.obj_ = __atomic_load_n(&(tuple->tsw_.obj_), __ATOMIC_ACQUIRE);
    if (v1 == v2 && !v1.lock) break;
    /**
     * else: update by new tsw_ and retry with using this one.
     */
    v1 = v2;
#if ADD_ANALYSIS
    ++tres_->local_extra_reads_;
#endif
  }

  this->appro_commit_ts_ = max(this->appro_commit_ts_, v1.wts);
  read_set_.emplace_back(key, tuple, return_val_, v1);

FINISH_READ:

#if ADD_ANALYSIS
  tres_->local_read_latency_ += rdtscp() - start;
#endif
  return;
}

void TxExecutor::write(uint64_t key) {
#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif

  // these variable cause error (-fpermissive)
  // "crosses initialization of ..."
  // So it locate before first goto instruction.
  TsWord tsword;

  if (searchWriteSet(key)) goto FINISH_WRITE;

  /**
   * Search tuple from data structure.
   */
  Tuple *tuple;
  SetElement<Tuple> *re;
  re = searchReadSet(key);
  if (re) {
    tuple = re->rcdptr_;
  } else {
#if MASSTREE_USE
    tuple = MT.get_value(key);
#if ADD_ANALYSIS
    ++tres_->local_tree_traversal_;
#endif
#else
    tuple = get_tuple(Table, key);
#endif
  }

  tsword.obj_ = __atomic_load_n(&(tuple->tsw_.obj_), __ATOMIC_ACQUIRE);
  this->appro_commit_ts_ = max(this->appro_commit_ts_, tsword.rts() + 1);
  write_set_.emplace_back(key, tuple, tsword);

FINISH_WRITE:;

#if ADD_ANALYSIS
  tres_->local_write_latency_ += rdtscp() - start;
#endif
}

bool TxExecutor::validationPhase() {
#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif

  lockWriteSet();
#if NO_WAIT_LOCKING_IN_VALIDATION
  if (this->status_ == TransactionStatus::aborted) {
#if ADD_ANALYSIS
    tres_->local_vali_latency_ += rdtscp() - start;
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
  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr)
    /**
     * Originally, commit_ts is calculated by two loops
     * (read set loop, write set loop).
     * However, it is bad for performances.
     * Write set loop should be merged with write set (lock) loop.
     * Read set loop should be merged with read set (validation) loop.
     * The result reduces two loops and improves performance.
     */
    commit_ts_ = max(commit_ts_, (*itr).tsw_.wts);

  // step3, validate the read set.
  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
    TsWord v1, v2;
#if ADD_ANALYSIS
    ++tres_->local_rtsupd_chances_;
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
            ++tres_->local_timestamp_history_success_counts_;
#endif
            break;
          }
          /**
           * Fail
           */
#if ADD_ANALYSIS
          ++tres_->local_timestamp_history_fail_counts_;
#endif
#endif
          // end timestamp history processing
#if ADD_ANALYSIS
          tres_->local_vali_latency_ += rdtscp() - start;
#endif
          unlockWriteSet();
          return false;
        }

        SetElement<Tuple> *inW = searchWriteSet((*itr).key_);
        if ((v1.rts()) < commit_ts_ && v1.lock) {
          if (inW == nullptr) {
            /**
             * It needs to update read timestamp.
             * However, v1 is locked and this transaction didn't lock it,
             * so other transaction locked.
             */
#if ADD_ANALYSIS
            tres_->local_vali_latency_ += rdtscp() - start;
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
            ++tres_->local_rtsupd_;
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
#if ADD_ANALYSIS
  tres_->local_vali_latency_ += rdtscp() - start;
#endif
  return true;
}

void TxExecutor::abort() {
  /**
   * Clean-up local read/write set.
   */
  read_set_.clear();
  write_set_.clear();

  ++tres_->local_abort_counts_;

#if BACK_OFF

#if ADD_ANALYSIS
  uint64_t start(rdtscp());
#endif
  Backoff::backoff(FLAGS_clocks_per_us);
#if ADD_ANALYSIS
  ++tres_->local_backoff_latency_ += rdtscp() - start;
#endif

#endif
}

void TxExecutor::writePhase() {
  TsWord result;

  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    /**
     * update payload.
     */
    memcpy((*itr).rcdptr_->val_, write_val_, VAL_SIZE);
    result.wts = this->commit_ts_;
    result.delta = 0;
    result.lock = 0;
#if TIMESTAMP_HISTORY
    __atomic_store_n(&((*itr).rcdptr_->pre_tsw_.obj_), (*itr).tsw_.obj_,
                     __ATOMIC_RELAXED);
#endif
    __atomic_store_n(&((*itr).rcdptr_->tsw_.obj_), result.obj_,
                     __ATOMIC_RELEASE);
  }

  /**
   * Clean-up local read/write/lock set.
   */
  read_set_.clear();
  write_set_.clear();
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
    expected.obj_ =
            __atomic_load_n(&((*itr).rcdptr_->tsw_.obj_), __ATOMIC_ACQUIRE);
    for (;;) {
      if (expected.lock) {
        if (this->wonly_ == false) {
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
                  ++tres_->local_timestamp_history_success_counts_;
#endif
                  continue;
                }
                /**
                 * Fail
                 */
#if ADD_ANALYSIS
                ++tres_->local_timestamp_history_fail_counts_;
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
    expected.obj_ = loadAcquire((*itr).rcdptr_->tsw_.obj_);
    desired.obj_ = expected.obj_;
    desired.lock = 0;
    storeRelease((*itr).rcdptr_->tsw_.obj_, desired.obj_);
  }
}

void TxExecutor::unlockWriteSet(std::vector<SetElement<Tuple>>::iterator end) {
  TsWord expected, desired;

  for (auto itr = write_set_.begin(); itr != end; ++itr) {
    expected.obj_ = loadAcquire((*itr).rcdptr_->tsw_.obj_);
    desired.obj_ = expected.obj_;
    desired.lock = 0;
    storeRelease((*itr).rcdptr_->tsw_.obj_, desired.obj_);
  }
}

void TxExecutor::dispWS() {
  cout << "th " << this->thid_ << ": write set: ";

  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    cout << "(" << (*itr).key_ << ", " << (*itr).val_ << "), ";
  }
  cout << endl;
}
