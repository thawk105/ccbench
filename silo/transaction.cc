#include <stdio.h>
#include <sys/time.h>
#include <xmmintrin.h>
#include <algorithm>
#include <bitset>
#include <fstream>
#include <sstream>
#include <string>

#include "include/atomic_tool.hh"
#include "include/common.hh"
#include "include/log.hh"
#include "include/transaction.hh"

#include "../include/backoff.hh"
#include "../include/debug.hh"
#include "../include/fileio.hh"
#include "../include/masstree_wrapper.hh"
#include "../include/tsc.hh"

extern bool chkSpan(struct timeval &start, struct timeval &stop,
                    long threshold);
extern void displayDB();

using namespace std;

void TxnExecutor::begin() {
  status_ = TransactionStatus::kInFlight;
  max_wset_.obj_ = 0;
  max_rset_.obj_ = 0;
}

WriteElement<Tuple> *TxnExecutor::searchWriteSet(uint64_t key) {
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    if ((*itr).key_ == key) return &(*itr);
  }

  return nullptr;
}

ReadElement<Tuple> *TxnExecutor::searchReadSet(uint64_t key) {
  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
    if ((*itr).key_ == key) return &(*itr);
  }

  return nullptr;
}

void TxnExecutor::read(uint64_t key) {
#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif

  // these variable cause error (-fpermissive)
  // "crosses initialization of ..."
  // So it locate before first goto instruction.
  Tidword expected, check;

  if (searchReadSet(key) || searchWriteSet(key)) goto FINISH_READ;

  Tuple *tuple;
#if MASSTREE_USE
  tuple = MT.get_value(key);
#if ADD_ANALYSIS
  ++sres_->local_tree_traversal_;
#endif
#else
  tuple = get_tuple(Table, key);
#endif

  //(a) reads the TID word, spinning until the lock is clear

  expected.obj_ = loadAcquire(tuple->tidword_.obj_);
  // check if it is locked.
  // spinning until the lock is clear

  for (;;) {
    while (expected.lock) {
      expected.obj_ = loadAcquire(tuple->tidword_.obj_);
    }

    //(b) checks whether the record is the latest version
    // omit. because this is implemented by single version

    //(c) reads the data
    memcpy(return_val_, tuple->val_, VAL_SIZE);

    //(d) performs a memory fence
    // don't need.
    // order of load don't exchange.

    //(e) checks the TID word again
    check.obj_ = loadAcquire(tuple->tidword_.obj_);
    if (expected == check) break;
    expected = check;
#if ADD_ANALYSIS
    ++sres_->local_extra_reads_;
#endif
  }

  read_set_.emplace_back(key, tuple, return_val_, expected);
  // emplace is often better performance than push_back.

#if SLEEP_READ_PHASE
  sleepTics(SLEEP_READ_PHASE);
#endif

FINISH_READ:

#if ADD_ANALYSIS
  sres_->local_read_latency_ += rdtscp() - start;
#endif
  return;
}

void TxnExecutor::write(uint64_t key) {
#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif

  if (searchWriteSet(key)) goto FINISH_WRITE;
  Tuple *tuple;
  ReadElement<Tuple> *re;
  re = searchReadSet(key);
  if (re) {
    tuple = re->rcdptr_;
  } else {
#if MASSTREE_USE
    tuple = MT.get_value(key);
#if ADD_ANALYSIS
    ++sres_->local_tree_traversal_;
#endif
#else
    tuple = get_tuple(Table, key);
#endif
  }

  write_set_.emplace_back(key, tuple);  // push の方が性能が良い

FINISH_WRITE:

#if ADD_ANALYSIS
  sres_->local_write_latency_ += rdtscp() - start;
#endif
  return;
}

bool TxnExecutor::validationPhase() {
#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif

  /* Phase 1
   * lock write_set_ sorted.*/
  sort(write_set_.begin(), write_set_.end());
  lockWriteSet();
#if NO_WAIT_LOCKING_IN_VALIDATION
  if (this->status_ == TransactionStatus::kAborted) return false;
#endif

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
    if ((*itr).tidword_.epoch != check.epoch ||
        (*itr).tidword_.tid != check.tid) {
#if ADD_ANALYSIS
      sres_->local_vali_latency_ += rdtscp() - start;
#endif
      this->status_ = TransactionStatus::kAborted;
      return false;
    }
    // 2
    // if (!check.latest) return false;

    // 3
    if (check.lock && !searchWriteSet((*itr).key_)) {
#if ADD_ANALYSIS
      sres_->local_vali_latency_ += rdtscp() - start;
#endif
      this->status_ = TransactionStatus::kAborted;
      return false;
    }
    max_rset_ = max(max_rset_, check);
  }

  // goto Phase 3
#if ADD_ANALYSIS
  sres_->local_vali_latency_ += rdtscp() - start;
#endif
  this->status_ = TransactionStatus::kCommitted;
  return true;
}

void TxnExecutor::abort() {
  unlockWriteSet();

  read_set_.clear();
  write_set_.clear();

#if BACK_OFF
#if ADD_ANALYSIS
  uint64_t start(rdtscp());
#endif

  Backoff::backoff(CLOCKS_PER_US);

#if ADD_ANALYSIS
  sres_->local_backoff_latency_ += rdtscp() - start;
#endif
#endif
}

void TxnExecutor::wal(uint64_t ctid) {
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    LogRecord log(ctid, (*itr).key_, write_val_);
    log_set_.push_back(log);
    latest_log_header_.chkSum += log.computeChkSum();
    ++latest_log_header_.logRecNum;
  }

  if (log_set_.size() > LOGSET_SIZE / 2) {
    // prepare write header
    latest_log_header_.convertChkSumIntoComplementOnTwo();

    // write header
    logfile_.write((void *)&latest_log_header_, sizeof(LogHeader));

    // write log record
    // for (auto itr = log_set_.begin(); itr != log_set_.end(); ++itr)
    //  logfile_.write((void *)&(*itr), sizeof(LogRecord));
    logfile_.write((void *)&(log_set_[0]),
                   sizeof(LogRecord) * latest_log_header_.logRecNum);

    // logfile_.fdatasync();

    // clear for next transactions.
    latest_log_header_.init();
    log_set_.clear();
  }
}

void TxnExecutor::writePhase() {
  // It calculates the smallest number that is
  //(a) larger than the TID of any record read or written by the transaction,
  //(b) larger than the worker's most recently chosen TID,
  // and (C) in the current global epoch.

  Tidword tid_a, tid_b, tid_c;

  // calculates (a)
  // about read_set_
  tid_a = max(max_wset_, max_rset_);
  tid_a.tid++;

  // calculates (b)
  // larger than the worker's most recently chosen TID,
  tid_b = mrctid_;
  tid_b.tid++;

  // calculates (c)
  tid_c.epoch = ThLocalEpoch[thid_].obj_;

  // compare a, b, c
  Tidword maxtid = max({tid_a, tid_b, tid_c});
  maxtid.lock = 0;
  maxtid.latest = 1;
  mrctid_ = maxtid;

  // wal(maxtid.obj_);

  // write(record, commit-tid)
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    // update and unlock
    memcpy((*itr).rcdptr_->val_, write_val_, VAL_SIZE);
    storeRelease((*itr).rcdptr_->tidword_.obj_, maxtid.obj_);
  }

  read_set_.clear();
  write_set_.clear();
}

void TxnExecutor::lockWriteSet() {
  Tidword expected, desired;

  this->lock_num_ = 0;
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    expected.obj_ = loadAcquire((*itr).rcdptr_->tidword_.obj_);
    for (;;) {
      if (expected.lock) {
#if NO_WAIT_LOCKING_IN_VALIDATION
        this->status_ = TransactionStatus::kAborted;
        return;
#endif
        expected.obj_ = loadAcquire((*itr).rcdptr_->tidword_.obj_);
      } else {
        desired = expected;
        desired.lock = 1;
        if (compareExchange((*itr).rcdptr_->tidword_.obj_, expected.obj_,
                            desired.obj_))
          break;
      }
    }
    ++this->lock_num_;

    max_wset_ = max(max_wset_, expected);
  }
}

void TxnExecutor::unlockWriteSet() {
  Tidword expected, desired;

  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
#if NO_WAIT_LOCKING_IN_VALIDATION
    if (this->lock_num_ == 0) return;
#endif
    expected.obj_ = loadAcquire((*itr).rcdptr_->tidword_.obj_);
    desired = expected;
    desired.lock = 0;
    storeRelease((*itr).rcdptr_->tidword_.obj_, desired.obj_);
    --this->lock_num_;
  }
}

void TxnExecutor::displayWriteSet() {
  printf("display_write_set()\n");
  printf("--------------------\n");
  for (auto &ws : write_set_) {
    printf("key\t:\t%lu\n", ws.key_);
  }
}
