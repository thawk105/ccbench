#include <algorithm>
#include <cmath>
#include <random>
#include <stdio.h>

#include "../include/debug.hpp"

#include "include/atomic_tool.hpp"
#include "include/transaction.hpp"
#include "include/tuple.hpp"

using namespace std;

ReadElement *
TxExecutor::searchReadSet(unsigned int key)
{
  for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
    if ((*itr).key == key) return &(*itr);
  }

  return nullptr;
}

WriteElement *
TxExecutor::searchWriteSet(unsigned int key)
{
  for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
    if ((*itr).key == key) return &(*itr);
  }

  return nullptr;
}

template <typename T> T*
TxExecutor::searchRLL(unsigned int key)
{
  // will do : binary search
  for (auto itr = RLL.begin(); itr != RLL.end(); ++itr) {
    if ((*itr).key == key) return &(*itr);
  }

  return nullptr;
}

void
TxExecutor::removeFromCLL(unsigned int key)
{
  int ctr = 0;
  for (auto itr = CLL.begin(); itr != CLL.end(); ++itr) {
    if ((*itr).key == key) break;
    else ctr++;
  }

  CLL.erase(CLL.begin() + ctr);
}

void
TxExecutor::begin()
{
  this->status = TransactionStatus::inFlight;
  this->max_rset.obj = 0;
  this->max_wset.obj = 0;

  return;
}

char*
TxExecutor::read(unsigned int key)
{
  Tuple *tuple = &Table[key];

  // tuple exists in write set.
  WriteElement *inW = searchWriteSet(key);
  //if (inW) return inW->val;
  if (inW) return returnVal;

  // tuple exists in read set.
  ReadElement *inR = searchReadSet(key);
  if (inR) return inR->val;

  // tuple doesn't exist in read/write set.
#ifdef RWLOCK
  LockElement<RWLock> *inRLL = searchRLL<LockElement<RWLock>>(key);
#endif // RWLOCK
#ifdef MQLOCK
  LockElement<MQLock> *inRLL = searchRLL<LockElement<MQLock>>(key);
#endif // MQLOCK

  Epotemp loadepot;
  loadepot.obj = __atomic_load_n(&(tuple->epotemp.obj), __ATOMIC_ACQUIRE);

  if (inRLL != nullptr) lock(key, tuple, inRLL->mode);
  else if (loadepot.temp >= TEMP_THRESHOLD) lock(key, tuple, false);
  if (this->status == TransactionStatus::aborted) return nullptr;
  
  Tidword expected, desired;
  for (;;) {
    expected.obj = __atomic_load_n(&(tuple->tidword.obj), __ATOMIC_ACQUIRE);

#ifdef RWLOCK
    while (expected.lock) {
#endif // RWLOCK
      // if you wait due to being write-locked, it may occur dead lock.
      // it need to guarantee that this parts definitely progress.
      // So it sholud wait expected.lock because it will be released definitely.
      //
      // if expected.lock raise, opponent worker entered pre-commit phase.
      expected.obj = __atomic_load_n(&(tuple->tidword.obj), __ATOMIC_ACQUIRE);
    }

    memcpy(returnVal, tuple->val, VAL_SIZE); // read

    desired.obj = __atomic_load_n(&(tuple->tidword.obj), __ATOMIC_ACQUIRE);
    if (expected == desired) break;
  }

  readSet.emplace_back(expected, key, returnVal);
  return returnVal;
}

void
TxExecutor::write(unsigned int key)
{
  // tuple exists in write set.
  WriteElement *inw = searchWriteSet(key);
  if (inw) return;

  Tuple *tuple = &Table[key];

#ifdef RWLOCK
  LockElement<RWLock> *inRLL = searchRLL<LockElement<RWLock>>(key);
#endif // RWLOCK
#ifdef MQLOCK
  LockElement<MQLock> *inRLL = searchRLL<LockElement<MQLock>>(key);
#endif // MQLOCK

  Epotemp loadepot;
  loadepot.obj = __atomic_load_n(&(tuple->epotemp.obj), __ATOMIC_ACQUIRE);

  if (inRLL || loadepot.temp >= TEMP_THRESHOLD) lock(key, tuple, true);
  if (this->status == TransactionStatus::aborted) return;
  
  //writeSet.emplace_back(key, writeVal);
  writeSet.emplace_back(key);
  return;
}

void
TxExecutor::lock(unsigned int key, Tuple *tuple, bool mode)
{
  unsigned int vioctr = 0;
  unsigned int threshold;
  bool upgrade = false;

#ifdef RWLOCK
  LockElement<RWLock> *le = nullptr;
#endif // RWLOCK
  // RWLOCK : アップグレードするとき，CLL ループで該当する
  // エレメントを記憶しておき，そのエレメントを更新するため．
  // MQLOCK : アップグレード機能が無いので，不要．
  // reader ロックを解放して，CLL から除去して，writer ロックをかける．

  // lock exists in CLL (current lock list)
  sort(CLL.begin(), CLL.end());
  for (auto itr = CLL.begin(); itr != CLL.end(); ++itr) {
    // lock already exists in CLL 
    //    && its lock mode is equal to needed mode or it is stronger than needed mode.
    if ((*itr).key == key) {
      if (mode == (*itr).mode || mode < (*itr).mode) return;
      else {
#ifdef RWLOCK
        le = &(*itr);
#endif // RWLOCK
        upgrade = true;
      }
    }

    // collect violation
    if ((*itr).key >= key) {
      if (vioctr == 0) threshold = (*itr).key;
      
      vioctr++;
    }

  }

  if (vioctr == 0) threshold = -1;

  // if too many violations
  // i set my condition of too many because the original paper of mocc didn't show
  // the condition of too many.
  //if ((CLL.size() / 2) < vioctr && CLL.size() >= (MAX_OPE / 2)) {
  // test condition. mustn't enter.
  if ((vioctr > 100)) {
#ifdef RWLOCK
    if (mode) {
      if (upgrade) {
        if (tuple->rwlock.upgrade()) {
          le->mode = true;
          return;
        } 
        else {
          this->status = TransactionStatus::aborted;
          return;
        }
      } 
      else if (tuple->rwlock.w_trylock()) {
        CLL.push_back(LockElement<RWLock>(key, &(tuple->rwlock), true));
        return;
      } 
      else {
        this->status = TransactionStatus::aborted;
        return;
      }
    } 
    else {
      if (tuple->rwlock.r_trylock()) {
        CLL.push_back(LockElement<RWLock>(key, &(tuple->rwlock), false));
        return;
      } 
      else {
        this->status = TransactionStatus::aborted;
        return;
      }
    }
#endif // RWLOCK
#ifdef MQLOCK
    if (mode) {
      if (upgrade) {
        tuple->mqlock.release_reader_lock(this->locknum, key);
        removeFromCLL(key);
        if (tuple->mqlock.acquire_writer_lock(this->locknum, key, true) == MQL_RESULT::Acquired) {
          CLL.push_back(LockElement<MQLock>(key, &(tuple->mqlock), true));
          return;
        }
        else {
          this->status = TransactionStatus::aborted;
          return;
        }
      }
      else if (tuple->mqlock.acquire_writer_lock(this->locknum, key, true) == MQL_RESULT::Acquired) {
        CLL.push_back(LockElement<MQLock>(key, &(tuple->mqlock), true));
        return;
      }
      else {
        this->status = TransactionStatus::aborted;
        return;
      }
    }
    else {
      if (tuple->mqlock.acquire_reader_lock(this->locknum, key, true) == MQL_RESULT::Acquired) {
        CLL.push_back(LockElement<MQLock>(key, &(tuple->mqlock), false));
        return;
      }
      else {
        this->status = TransactionStatus::aborted;
        return;
      }
    }
#endif // MQLOCK
  }
  
  if (vioctr != 0) {
    // not in canonical mode. restore.
    for (auto itr = CLL.begin() + (CLL.size() - vioctr); itr != CLL.end(); ++itr) {
#ifdef RWLOCK
      if ((*itr).mode) {
        (*itr).lock->w_unlock();
      }
      else (*itr).lock->r_unlock();
#endif // RWLOCK

#ifdef MQLOCK
      if ((*itr).mode) {
        (*itr).lock->release_writer_lock(this->locknum, key);
      }
      else (*itr).lock->release_reader_lock(this->locknum, key);
#endif // MQLOCK
    }
      
    //delete from CLL
    if (CLL.size() == vioctr) CLL.clear();
    else CLL.erase(CLL.begin() + (CLL.size() - vioctr), CLL.end());
  }

  // unconditional lock in canonical mode.
  for (auto itr = RLL.begin(); itr != RLL.end(); ++itr) {
    if ((*itr).key <= threshold) continue;
    if ((*itr).key < key) {
#ifdef RWLOCK
      if ((*itr).mode) {
        (*itr).lock->w_lock();
      }
      else (*itr).lock->r_lock();
#endif // RWLOCK

#ifdef MQLOCK
      if ((*itr).mode) {
        (*itr).lock->acquire_writer_lock(this->locknum, key);
      }
      else (*itr).lock->acquire_reader_lock(this->locknum, key);
#endif // MQLOCK
      CLL.push_back(*itr);
    } else break;
  }

#ifdef RWLOCK
  if (mode) {
    tuple->rwlock.w_lock(); 
  }
  else tuple->rwlock.r_lock();
  CLL.push_back(LockElement<RWLock>(key, &(tuple->rwlock), mode));
  return;
#endif // RWLOCK

#ifdef MQLOCK
  if (mode) {
    tuple->mqlock.acquire_writer_lock(this->locknum, key, false);
  }
  else tuple->mqlock.acquire_reader_lock(this->locknum, key, false);
  CLL.push_back(LockElement<MQLock>(key, &(tuple->mqlock), mode));
  return;
#endif // MQLOCK
}

void
TxExecutor::construct_RLL()
{
  Tuple *tuple;
  RLL.clear();
  
  for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
#ifdef RWLOCK
    RLL.push_back(LockElement<RWLock>((*itr).key, &(Table[(*itr).key].rwlock), true));
#endif // RWLOCK
#ifdef MQLOCK
    RLL.push_back(LockElement<MQLock>((*itr).key, &(Table[(*itr).key].mqlock), true));
#endif // MQLOCK
  }

  for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
    // check whether itr exists in RLL
#ifdef RWLOCK
    if (searchRLL<LockElement<RWLock>>((*itr).key) != nullptr) continue;
#endif // RWLOCK
#ifdef MQLOCK
    if (searchRLL<LockElement<MQLock>>((*itr).key) != nullptr) continue;
#endif // MQLOCK

    // r not in RLL
    // if temprature >= threshold
    //  || r failed verification
    Epotemp loadepot;
    tuple = &Table[(*itr).key];
    loadepot.obj = __atomic_load_n(&(tuple->epotemp.obj), __ATOMIC_ACQUIRE);
    if (loadepot.temp >= TEMP_THRESHOLD 
        || (*itr).failed_verification) {
#ifdef RWLOCK
      RLL.push_back(LockElement<RWLock>((*itr).key, &(tuple->rwlock), false));
#endif // RWLOCK
#ifdef MQLOCK
      RLL.push_back(LockElement<MQLock>((*itr).key, &(tuple->mqlock), false));
#endif // MQLOCK
    }

    // maintain temprature p
    if ((*itr).failed_verification) {
      Epotemp expected, desired;
      uint64_t nowepo;
      expected.obj = __atomic_load_n(&(tuple->epotemp.obj), __ATOMIC_ACQUIRE);
      nowepo = (loadAcquireGE()).obj;

      if (expected.epoch != nowepo) {
        desired.epoch = nowepo;
        desired.temp = 0;
        __atomic_store_n(&(tuple->epotemp.obj), desired.obj, __ATOMIC_RELEASE);
      }

      for (;;) {
        if (expected.temp == TEMP_MAX) break;
        else if (rnd->next() % (1 << expected.temp) == 0) {
          desired = expected;
          desired.temp = expected.temp + 1;
        } else break;
        if (__atomic_compare_exchange_n(&(tuple->epotemp.obj), &(expected.obj), desired.obj, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) break;
      }
    }
  }

  sort(RLL.begin(), RLL.end());
    
  return;
}

bool
TxExecutor::commit()
{
  Tuple *tuple;
  Tidword expected, desired;

  // phase 1 lock write set.
  sort(writeSet.begin(), writeSet.end());
  for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
    tuple = &Table[(*itr).key];

    lock((*itr).key, tuple, true);
    if (this->status == TransactionStatus::aborted) {
      return false;
    }

    this->max_wset = max(this->max_wset, tuple->tidword);
  }

  asm volatile ("" ::: "memory");
  __atomic_store_n(&(ThLocalEpoch[thid].obj), (loadAcquireGE()).obj, __ATOMIC_RELEASE);
  asm volatile ("" ::: "memory");

  for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
    Tidword check;
    tuple = &Table[(*itr).key];
    check.obj = __atomic_load_n(&(tuple->tidword.obj), __ATOMIC_ACQUIRE);
    if ((*itr).tidword.epoch != check.epoch || (*itr).tidword.tid != check.tid) {
      (*itr).failed_verification = true;
      this->status = TransactionStatus::aborted;
      return false;
    }

    // Silo protocol
#ifdef RWLOCK
    if (tuple->rwlock.counter.load(memory_order_acquire) == -1) {
#endif // RWLOCK
      WriteElement *inW = searchWriteSet((*itr).key);
      //if the rwlock is already acquired and the owner isn't me, abort.
      if (inW == nullptr) {
        this->status = TransactionStatus::aborted;
        return false;
      }
    }
    this->max_rset = max(this->max_rset, tuple->tidword);
  }

  for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr)
    Table[(*itr).key].tidword.upLockBits();

  return true;
}

void
TxExecutor::abort()
{
  //unlock CLL
  unlockCLL();
  
  construct_RLL();

  readSet.clear();
  writeSet.clear();
  return;
}

void
TxExecutor::unlockCLL()
{
  Tidword expected, desired;

  for (auto itr = CLL.begin(); itr != CLL.end(); ++itr) {
#ifdef RWLOCK
    if ((*itr).mode) {
      (*itr).lock->w_unlock();
    }
    else (*itr).lock->r_unlock();
#endif // RWLOCK

#ifdef MQLOCK
    if ((*itr).mode) {

      (*itr).lock->release_writer_lock(this->locknum, (*itr).key);
    }
    else (*itr).lock->release_reader_lock(this->locknum, (*itr).key);
#endif
  }
  CLL.clear();
}

void
TxExecutor::writePhase()
{
  Tidword tid_a, tid_b, tid_c;

  // calculates (a)
  tid_a = max(this->max_rset, this->max_wset);
  tid_a.tid++;

  //calculates (b)
  //larger than the worker's most recently chosen TID,
  tid_b = mrctid;
  tid_b.tid++;

  // calculates (c)
  tid_c.epoch = ThLocalEpoch[thid].obj;

  // compare a, b, c
  Tidword maxtid = max({tid_a, tid_b, tid_c});
  maxtid.lock = 0;
  mrctid = maxtid;

  //write (record, commit-tid)
  for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
    //update and down lockBit
    memcpy(Table[(*itr).key].val, writeVal, VAL_SIZE);
    __atomic_store_n(&(Table[(*itr).key].tidword.obj), maxtid.obj, __ATOMIC_RELEASE);
  }

  unlockCLL();
  RLL.clear();
  readSet.clear();
  writeSet.clear();
}

void
TxExecutor::dispCLL()
{
  cout << "th " << this->thid << ": CLL: ";
  for (auto itr = CLL.begin(); itr != CLL.end(); ++itr) {
    cout << (*itr).key << "(";
    if ((*itr).mode) cout << "w) ";
    else cout << "r) ";
  }
  cout << endl;
}

void
TxExecutor::dispRLL()
{
  cout << "th " << this->thid << ": RLL: ";
  for (auto itr = RLL.begin(); itr != RLL.end(); ++itr) {
    cout << (*itr).key << "(";
    if ((*itr).mode) cout << "w) ";
    else cout << "r) ";
  }
  cout << endl;
}

void
TxExecutor::dispWS()
{
  cout << "th " << this->thid << ": write set: ";
  for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
    //cout << "(" << (*itr).key << ", " << (*itr).val << "), ";
    cout << "(" << (*itr).key << ", " << writeVal << "), ";
  }
  cout << endl;
}

