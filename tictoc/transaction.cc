
#include <stdio.h>
#include <sys/time.h>

#include <algorithm>
#include <bitset>
#include <fstream>
#include <sstream>
#include <string>

#include "../include/backoff.hh"
#include "../include/debug.hh"
#include "../include/inline.hh"
#include "../include/masstree_wrapper.hh"
#include "../include/tsc.hh"
#include "include/common.hh"
#include "include/transaction.hh"
#include "include/tuple.hh"

using namespace std;

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
    // it must check whether this write set include the tuple,
    // but it already checked L31 - L33.
    // so it must abort.
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

  // these variable cause error (-fpermissive)
  // "crosses initialization of ..."
  // So it locate before first goto instruction.
  TsWord v1, v2;

  if (searchReadSet(key) || searchWriteSet(key)) goto FINISH_READ;

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
#if PREEMPTIVE_ABORTS
      if (preemptiveAborts(v1)) goto FINISH_READ;
#endif
      v1.obj_ = __atomic_load_n(&(tuple->tsw_.obj_), __ATOMIC_ACQUIRE);
      continue;
    }

    memcpy(return_val_, tuple->val_, VAL_SIZE);

    v2.obj_ = __atomic_load_n(&(tuple->tsw_.obj_), __ATOMIC_ACQUIRE);
    if (v1 == v2 && !v1.lock) break;
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

FINISH_WRITE:

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

  // logically, it must execute full fence here,
  // while we assume intel architecture and CAS(cmpxchg) in lockWriteSet() did
  // it.
  //
  asm volatile("" ::: "memory");

  // step2, compute the commit timestamp
  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr)
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
#if ADD_ANALYSIS
            ++tres_->local_timestamp_history_success_counts_;
#endif
            break;
          }
#if ADD_ANALYSIS
          ++tres_->local_timestamp_history_fail_counts_;
#endif
#endif
          // end timestamp history processing
#if ADD_ANALYSIS
          tres_->local_vali_latency_ += rdtscp() - start;
#endif
          return false;
        }

        SetElement<Tuple> *inW = searchWriteSet((*itr).key_);
        if ((v1.rts()) < commit_ts_ && v1.lock) {
          if (inW == nullptr) {
#if ADD_ANALYSIS
            tres_->local_vali_latency_ += rdtscp() - start;
#endif
            return false;
          }
        }

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
  unlockCLL();

  read_set_.clear();
  write_set_.clear();
  cll_.clear();
  ++tres_->local_abort_counts_;

#if BACK_OFF

#if ADD_ANALYSIS
  uint64_t start(rdtscp());
#endif

  Backoff::backoff(CLOCKS_PER_US);

#if ADD_ANALYSIS
  ++tres_->local_backoff_latency_ += rdtscp() - start;
#endif

#endif
}

void TxExecutor::writePhase() {
  TsWord result;

  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
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

  read_set_.clear();
  write_set_.clear();
  cll_.clear();
  ++tres_->local_commit_counts_;
}

void TxExecutor::lockWriteSet() {
  TsWord expected, desired;

  sort(write_set_.begin(), write_set_.end());
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    expected.obj_ =
        __atomic_load_n(&((*itr).rcdptr_->tsw_.obj_), __ATOMIC_ACQUIRE);
    for (;;) {
      /* no-wait locking in validation */
      if (expected.lock) {
#if NO_WAIT_LOCKING_IN_VALIDATION
        if (this->wonly_ == false) {
          this->status_ = TransactionStatus::aborted;
          unlockCLL();
          return;
        }
#endif
        expected.obj_ =
            __atomic_load_n(&((*itr).rcdptr_->tsw_.obj_), __ATOMIC_ACQUIRE);
      } else {
        desired = expected;
        desired.lock = 1;
        if (__atomic_compare_exchange_n(&((*itr).rcdptr_->tsw_.obj_),
                                        &(expected.obj_), desired.obj_, false,
                                        __ATOMIC_RELAXED, __ATOMIC_RELAXED))
          break;
      }
    }

    commit_ts_ = max(commit_ts_, desired.rts() + 1);
    cll_.emplace_back((*itr).key_, (*itr).rcdptr_);
  }
}

void TxExecutor::unlockCLL() {
  TsWord expected, desired;

  for (auto itr = cll_.begin(); itr != cll_.end(); ++itr) {
    expected.obj_ =
        __atomic_load_n(&((*itr).rcdptr_->tsw_.obj_), __ATOMIC_ACQUIRE);
    desired.obj_ = expected.obj_;
    desired.lock = 0;
    __atomic_store_n(&((*itr).rcdptr_->tsw_.obj_), desired.obj_,
                     __ATOMIC_RELEASE);
  }
}

void TxExecutor::dispWS() {
  cout << "th " << this->thid_ << ": write set: ";

  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    cout << "(" << (*itr).key_ << ", " << (*itr).val_ << "), ";
  }
  cout << endl;
}
