
#include <stdio.h>
#include <sys/time.h>

#include <algorithm>
#include <bitset>
#include <fstream>
#include <sstream>
#include <string>

#include "include/common.hpp"
#include "include/transaction.hpp"
#include "include/tuple.hpp"

#include "../include/debug.hpp"
#include "../include/masstree_wrapper.hpp"
#include "../include/inline.hpp"

extern bool chkSpan(struct timeval &start, struct timeval &stop, long threshold);
extern void displayDB();

using namespace std;

SetElement<Tuple> *
TxExecutor::searchWriteSet(uint64_t key)
{
  for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
    if ((*itr).key == key) return &(*itr);
  }

  return nullptr;
}

SetElement<Tuple> *
TxExecutor::searchReadSet(uint64_t key)
{
  for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
    if ((*itr).key == key) return &(*itr);
  }

  return nullptr;
}

void
TxExecutor::tbegin()
{
  this->status = TransactionStatus::inFlight;
  this->commit_ts = 0;
  this->appro_commit_ts = 0;
}

bool
TxExecutor::preemptive_aborts(const TsWord& v1)
{
  if (v1.rts() < this->appro_commit_ts) {
    // it must check whether this write set include the tuple,
    // but it already checked L31 - L33.
    // so it must abort.
    this->status = TransactionStatus::aborted;
    ++tres->local_preemptive_aborts_counts;
    return true;
  }

  return false;
}

char*
TxExecutor::tread(uint64_t key)
{
  SetElement<Tuple> *result;

  result = searchWriteSet(key);
  if (result != nullptr) return writeVal;

  result = searchReadSet(key);
  if (result != nullptr) return result->val;

  TsWord v1, v2;

#if MASSTREE_USE
  Tuple *tuple = MT.get_value(key);
#else
  Tuple *tuple = get_tuple(Table, key);
#endif

  v1.obj = __atomic_load_n(&(tuple->tsw.obj), __ATOMIC_ACQUIRE);
  for (;;) {
    if (v1.lock) {
#if PREEMPTIVE_ABORTS
      if (preemptive_aborts(v1)) return 0;
#endif
      v1.obj = __atomic_load_n(&(tuple->tsw.obj), __ATOMIC_ACQUIRE);
      continue;
    }

    memcpy(returnVal, tuple->val, VAL_SIZE);

    v2.obj = __atomic_load_n(&(tuple->tsw.obj), __ATOMIC_ACQUIRE);
    if (v1 == v2 && !v1.lock) break;
    v1 = v2;
  } 

  this->appro_commit_ts = max(this->appro_commit_ts, v1.wts);
  readSet.emplace_back(key, tuple, returnVal, v1);

  return returnVal;
}

void 
TxExecutor::twrite(uint64_t key)
{
  SetElement<Tuple> *result;

  result = searchWriteSet(key);
  if (result != nullptr) return;

  TsWord tsword;
#if MASSTREE_USE
  Tuple *tuple = MT.get_value(key);
#else
  Tuple *tuple = get_tuple(Table, key);
#endif

  tsword.obj = __atomic_load_n(&(tuple->tsw.obj), __ATOMIC_ACQUIRE);
  this->appro_commit_ts = max(this->appro_commit_ts, tsword.rts() + 1);
  writeSet.emplace_back(key, tuple, tsword);
}

bool 
TxExecutor::validationPhase()
{
  lockWriteSet();
  if (this->status == TransactionStatus::aborted) {
    return false;
  }

  // logically, it must execute full fence here,
  // while we assume intel architecture and CAS(cmpxchg) in lockWriteSet() did it.
  //
  asm volatile ("" ::: "memory");

  // step2, compute the commit timestamp
  for (auto itr = readSet.begin(); itr != readSet.end(); ++itr)
    commit_ts = max(commit_ts, (*itr).tsw.wts);


  // step3, validate the read set.
  for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
    TsWord v1, v2;
    ++tres->local_rtsupd_chances;

    v1.obj  = __atomic_load_n(&((*itr).rcdptr->tsw.obj), __ATOMIC_ACQUIRE);
    for (;;) {
      if ((*itr).tsw.wts != v1.wts) {
#if TIMESTAMP_HISTORY
        TsWord pre_v1;
        pre_v1.obj = __atomic_load_n(&((*itr).rcdptr->pre_tsw.obj), __ATOMIC_ACQUIRE);
        if (pre_v1.wts <= commit_ts && commit_ts < v1.wts) {
          ++tres->local_timestamp_history_success_counts;
          break;
        }
        ++tres->local_timestamp_history_fail_counts;
#endif
        return false;
      }

      SetElement<Tuple> *inW = searchWriteSet((*itr).key);
      if ((v1.rts()) < commit_ts && v1.lock) {
        if (inW == nullptr) return false;
      }

      if (inW != nullptr) break;
      //extend the rts of the tuple
      if ((v1.rts()) < commit_ts) {
        // Handle delta overflow
        uint64_t delta = commit_ts - v1.wts;
        uint64_t shift = delta - (delta & 0x7fff);
        v2.obj = v1.obj;
        v2.wts = v2.wts + shift;
        v2.delta = delta - shift;
        if (__atomic_compare_exchange_n(&((*itr).rcdptr->tsw.obj), &(v1.obj), v2.obj, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
          ++tres->local_rtsupd;
          break;
        }
        else continue;
      } else {
        break;
      }
    }
  }

  return true;
}

void 
TxExecutor::abort() 
{
  unlockCLL();

  readSet.clear();
  writeSet.clear();
  cll.clear();
}

void 
TxExecutor::writePhase()
{
  TsWord result;

  for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
    memcpy((*itr).rcdptr->val, writeVal, VAL_SIZE);
    result.wts = this->commit_ts;
    result.delta = 0;
    result.lock = 0;
#if TIMESTAMP_HISTORY
    __atomic_store_n(&((*itr).rcdptr->pre_tsw.obj), (*itr).tsw.obj, __ATOMIC_RELAXED);
#endif
    __atomic_store_n(&((*itr).rcdptr->tsw.obj), result.obj, __ATOMIC_RELEASE);

  }

  readSet.clear();
  writeSet.clear();
  cll.clear();
}

void 
TxExecutor::lockWriteSet()
{
  TsWord expected, desired;

  sort(writeSet.begin(), writeSet.end());
  for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
    expected.obj = __atomic_load_n(&((*itr).rcdptr->tsw.obj), __ATOMIC_ACQUIRE);
    for (;;) {
      //no-wait locking in validation
      //ロックオブジェクトを作ってデストラクタで解放
      //オブジェクトの中身はtsw へのポインタ
      if (expected.lock) {
        if (this->wonly == false) {
          this->status = TransactionStatus::aborted;
          unlockCLL();
          return;
        }
      }
      desired = expected;
      desired.lock = 1;
      if (__atomic_compare_exchange_n(&((*itr).rcdptr->tsw.obj), &(expected.obj), desired.obj, false, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) break;
    }

    commit_ts = max(commit_ts, desired.rts() + 1);
    cll.emplace_back((*itr).key, (*itr).rcdptr);
  }
}

void 
TxExecutor::unlockCLL()
{
  TsWord expected, desired;

  for (auto itr = cll.begin(); itr != cll.end(); ++itr) {
    expected.obj = __atomic_load_n(&((*itr).rcdptr->tsw.obj), __ATOMIC_ACQUIRE);
    desired.obj = expected.obj;
    desired.lock = 0;
    __atomic_store_n(&((*itr).rcdptr->tsw.obj), desired.obj, __ATOMIC_RELEASE);
  }
}

void
TxExecutor::dispWS()
{
  cout << "th " << this->thid << ": write set: ";

  for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
    cout << "(" << (*itr).key << ", " << (*itr).val << "), ";
  }
  cout << endl;
}
