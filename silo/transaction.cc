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

#include "../include/debug.hh"
#include "../include/fileio.hh"
#include "../include/masstree_wrapper.hh"
#include "../include/tsc.hh"

extern bool chkSpan(struct timeval &start, struct timeval &stop,
                    long threshold);
extern void displayDB();

using namespace std;

void TxnExecutor::tbegin() {
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

char *TxnExecutor::tread(uint64_t key) {
#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif

#if MASSTREE_USE
  Tuple *tuple = MT.get_value(key);
#if ADD_ANALYSIS
  ++sres_->local_tree_traversal_;
#endif
#else
  Tuple *tuple = get_tuple(Table, key);
#endif

  // w
  WriteElement<Tuple> *inW = searchWriteSet(key);
  if (inW) {
#if ADD_ANALYSIS
    sres_->local_read_latency_ += rdtscp() - start;
#endif
    return write_val_;
  }

  // r
  ReadElement<Tuple> *inR = searchReadSet(key);
  if (inR) {
#if ADD_ANALYSIS
    sres_->local_read_latency_ += rdtscp() - start;
#endif
    return inR->val_;
  }

  //(a) reads the TID word, spinning until the lock is clear
  Tidword expected, check;

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

  read_set_.emplace_back(key, tuple, return_val_,
                         expected);
  // emplace is often better performance than push_back.

#if SLEEP_READ_PHASE
  sleepTics(SLEEP_READ_PHASE);
#endif

#if ADD_ANALYSIS
  sres_->local_read_latency_ += rdtscp() - start;
#endif
  return return_val_;
}

void TxnExecutor::twrite(uint64_t key) {
#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif

  WriteElement<Tuple> *inW = searchWriteSet(key);
  if (inW) {
#if ADD_ANALYSIS
    sres_->local_write_latency_ += rdtscp() - start;
#endif
    return;
  }

#if MASSTREE_USE
  Tuple *tuple = MT.get_value(key);
#if ADD_ANALYSIS
  ++sres_->local_tree_traversal_;
#endif
#else
  Tuple *tuple = get_tuple(Table, key);
#endif

  write_set_.emplace_back(key, tuple);  // push の方が性能が良い
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
      return false;
    }
    // 2
    // if (!check.latest) return false;

    // 3
    if (check.lock && !searchWriteSet((*itr).key_)) {
#if ADD_ANALYSIS
      sres_->local_vali_latency_ += rdtscp() - start;
#endif
      return false;
    }
    max_rset_ = max(max_rset_, check);
  }

  // goto Phase 3
#if ADD_ANALYSIS
  sres_->local_vali_latency_ += rdtscp() - start;
#endif
  return true;
}

void TxnExecutor::abort() {
  unlockWriteSet();

  read_set_.clear();
  write_set_.clear();
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

  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    expected.obj_ = loadAcquire((*itr).rcdptr_->tidword_.obj_);
    for (;;) {
      if (expected.lock) {
        expected.obj_ = loadAcquire((*itr).rcdptr_->tidword_.obj_);
      } else {
        desired = expected;
        desired.lock = 1;
        if (compareExchange((*itr).rcdptr_->tidword_.obj_, expected.obj_,
                            desired.obj_))
          break;
      }
    }

    max_wset_ = max(max_wset_, expected);
  }
}

void TxnExecutor::unlockWriteSet() {
  Tidword expected, desired;

  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    expected.obj_ = loadAcquire((*itr).rcdptr_->tidword_.obj_);
    desired = expected;
    desired.lock = 0;
    storeRelease((*itr).rcdptr_->tidword_.obj_, desired.obj_);
  }
}

void TxnExecutor::displayWriteSet() {
  printf("display_write_set()\n");
  printf("--------------------\n");
  for (auto &ws : write_set_) {
    printf("key\t:\t%lu\n", ws.key_);
  }
}
