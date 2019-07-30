
#include <string.h>
#include <algorithm>
#include <atomic>
#include <bitset>

#include "../include/atomic_wrapper.hh"
#include "../include/debug.hh"
#include "../include/tsc.hh"
#include "include/common.hh"
#include "include/transaction.hh"
#include "include/version.hh"

using namespace std;

inline SetElement<Tuple> *TxExecutor::searchReadSet(uint64_t key) {
  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
    if ((*itr).key_ == key) return &(*itr);
  }

  return nullptr;
}

inline SetElement<Tuple> *TxExecutor::searchWriteSet(uint64_t key) {
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    if ((*itr).key_ == key) return &(*itr);
  }

  return nullptr;
}

void TxExecutor::tbegin() {
#ifdef CCTR_ON
  TransactionTable *newElement, *tmt;

  tmt = loadAcquire(TMT[thid_]);
  if (this->status_ == TransactionStatus::committed) {
    this->txid_ = cstamp_;
    newElement = new TransactionTable(0, cstamp_);
  } else {
    this->txid_ = TMT[thid_]->lastcstamp_.load(memory_order_acquire);
    newElement = new TransactionTable(
        0, tmt->lastcstamp_.load(std::memory_order_acquire));
  }

  for (unsigned int i = 1; i < THREAD_NUM; ++i) {
    if (thid_ == i) continue;
    do {
      tmt = loadAcquire(TMT[i]);
    } while (tmt == nullptr);
    this->txid_ = max(this->txid_, tmt->lastcstamp_.load(memory_order_acquire));
  }
  this->txid_ += 1;
  newElement->txid_ = this->txid_;

  TransactionTable *expected, *desired;
  tmt = loadAcquire(TMT[thid_]);
  expected = tmt;
  gcobject_.gcq_for_TMT_.push_back(expected);

  for (;;) {
    desired = newElement;
    if (compareExchange(TMT[thid_], expected, desired)) break;
  }
#endif  // CCTR_ON

#ifdef CCTR_TW
  this->txid_ = ++CCtr;
  TMT[thid_]->txid_.store(this->txid_, std::memory_order_release);
#endif  // CCTR_TW

  status_ = TransactionStatus::inFlight;
}

char *TxExecutor::tread(uint64_t key) {
#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif

  // if it already access the key object once.
  SetElement<Tuple> *inW = searchWriteSet(key);
  if (inW) {
#if ADD_ANALYSIS
    sres_->local_read_latency_ += rdtscp() - start;
#endif
    return write_val_;
  }

  SetElement<Tuple> *inR = searchReadSet(key);
  if (inR) {
#if ADD_ANALYSIS
    sres_->local_read_latency_ += rdtscp() - start;
#endif
    return inR->ver_->val_;
  }

#if MASSTREE_USE
  Tuple *tuple = MT.get_value(key);
#if ADD_ANALYSIS
  ++sres_->local_tree_traversal_;
#endif
#else
  Tuple *tuple = get_tuple(Table, key);
#endif

  // if v not in t.writes:
  Version *ver = tuple->latest_.load(std::memory_order_acquire);
  if (ver->status_.load(memory_order_acquire) != VersionStatus::committed) {
    ver = ver->committed_prev_;
  }

  while (txid_ < ver->cstamp_.load(memory_order_acquire)) {
    // printf("txid_ %d, (verCstamp >> 1) %d\n", txid_, verCstamp >> 1);
    // fflush(stdout);
    ver = ver->committed_prev_;
    if (ver == nullptr) {
      ERR;
    }
  }

  read_set_.emplace_back(key, tuple, ver);

  // for fairness
  // ultimately, it is wasteful in prototype system.
  memcpy(return_val_, ver->val_, VAL_SIZE);

#if ADD_ANALYSIS
  sres_->local_read_latency_ += rdtscp() - start;
#endif
  return ver->val_;
}

void TxExecutor::twrite(uint64_t key) {
#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif

  // if it already wrote the key object once.
  SetElement<Tuple> *inW = searchWriteSet(key);
  if (inW) {
#if ADD_ANALYSIS
    sres_->local_write_latency_ += rdtscp() - start;
#endif
    return;
  }

  // if v not in t.writes:
  //
  // first-updater-wins rule
  // Forbid a transaction to update  a record that has a committed head version
  // later than its begin timestamp.

  Version *expected, *desired;
  desired = new Version();
  desired->cstamp_.store(
      this->txid_,
      memory_order_relaxed);  // storing before CAS because it will be accessed
                              // from read operation, write operation and
                              // garbage collection.
  desired->status_.store(VersionStatus::inFlight, memory_order_relaxed);

#if MASSTREE_USE
  Tuple *tuple = MT.get_value(key);
#if ADD_ANALYSIS
  ++sres_->local_tree_traversal_;
#endif
#else
  Tuple *tuple = get_tuple(Table, key);
#endif

  Version *vertmp;
  expected = tuple->latest_.load(std::memory_order_acquire);
  for (;;) {
    // w-w conflict with concurrent transactions.
    if (expected->status_.load(memory_order_acquire) ==
        VersionStatus::inFlight) {
      uint64_t rivaltid = expected->cstamp_.load(memory_order_acquire);
      if (this->txid_ <= rivaltid) {
        // if (1) { // no-wait で abort させても性能劣化はほぼ起きていない．
        //性能が向上されるケースもある．
        this->status_ = TransactionStatus::aborted;
        delete desired;
#if ADD_ANALYSIS
        sres_->local_write_latency_ += rdtscp() - start;
#endif
        return;
      }

      expected = tuple->latest_.load(std::memory_order_acquire);
      continue;
    }

    // if a head version isn't committed version
    vertmp = expected;
    while (vertmp->status_.load(memory_order_acquire) !=
           VersionStatus::committed) {
      vertmp = vertmp->committed_prev_;
      if (vertmp == nullptr) ERR;
    }

    // vertmp is committed latest_ version.
    if (txid_ < vertmp->cstamp_.load(memory_order_acquire)) {
      //  write - write conflict, first-updater-wins rule.
      // Writers must abort if they would overwirte a version created after
      // their snapshot.
      this->status_ = TransactionStatus::aborted;
      delete desired;
#if ADD_ANALYSIS
      sres_->local_write_latency_ += rdtscp() - start;
#endif
      return;
    }

    desired->prev_ = expected;
    desired->committed_prev_ = vertmp;
    if (tuple->latest_.compare_exchange_strong(
            expected, desired, memory_order_acq_rel, memory_order_acquire))
      break;
  }

#if ADD_ANALYSIS
  sres_->local_write_latency_ += rdtscp() - start;
#endif
  write_set_.emplace_back(key, tuple, desired);
}

void TxExecutor::commit() {
  this->cstamp_ = ++CCtr;
  status_ = TransactionStatus::committed;

  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    (*itr).ver_->cstamp_.store(this->cstamp_, memory_order_release);
    memcpy((*itr).ver_->val_, write_val_, VAL_SIZE);
    (*itr).ver_->status_.store(VersionStatus::committed, memory_order_release);
    gcobject_.gcq_for_versions_.push_back(
        GCElement((*itr).key_, (*itr).rcdptr_, (*itr).ver_, cstamp_));
  }

  read_set_.clear();
  write_set_.clear();

#ifdef CCTR_TW
  TMT[thid_]->lastcstamp_.store(this->cstamp_, std::memory_order_release);
#endif  // CCTR_TW

  ++sres_->local_commit_counts_;
  return;
}

void TxExecutor::abort() {
  status_ = TransactionStatus::aborted;

  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    (*itr).ver_->status_.store(VersionStatus::aborted, memory_order_release);
    gcobject_.gcq_for_versions_.push_back(
        GCElement((*itr).key_, (*itr).rcdptr_, (*itr).ver_, this->txid_));
  }

  read_set_.clear();
  write_set_.clear();
  ++sres_->local_abort_counts_;
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

void TxExecutor::mainte() {
  gcstop_ = rdtscp();
  if (chkClkSpan(gcstart_, gcstop_, GC_INTER_US * CLOCKS_PER_US)) {
    uint32_t load_threshold = gcobject_.getGcThreshold();
    if (pre_gc_threshold_ != load_threshold) {
#if ADD_ANALYSIS
      ++sres_->local_gc_counts_;
      uint64_t start = rdtscp();
#endif
      gcobject_.gcVersion(sres_);
      pre_gc_threshold_ = load_threshold;
      gcstart_ = gcstop_;
#ifdef CCTR_ON
      gcobject_.gcTMTElements(sres_);
#endif
#if ADD_ANALYSIS
      sres_->local_gc_latency_ += rdtscp() - start;
#endif
    }
  }
}
