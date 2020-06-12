
#include <stdio.h>
#include <algorithm>
#include <cmath>
#include <random>
#include <thread>

#include "../include/atomic_wrapper.hh"
#include "../include/backoff.hh"
#include "../include/debug.hh"
#include "../include/tsc.hh"
#include "include/atomic_tool.hh"
#include "include/transaction.hh"
#include "include/tuple.hh"

using namespace std;

/**
 * @brief Search xxx set
 * @detail Search element of local set corresponding to given key.
 * In this prototype system, the value to be updated for each worker thread 
 * is fixed for high performance, so it is only necessary to check the key match.
 * @param Key [in] the key of key-value
 * @return Corresponding element of local set
 */
ReadElement<Tuple> *TxExecutor::searchReadSet(uint64_t key) {
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
WriteElement<Tuple> *TxExecutor::searchWriteSet(uint64_t key) {
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    if ((*itr).key_ == key) return &(*itr);
  }

  return nullptr;
}

/**
 * @brief Search element from retrospective lock list.
 * @param key [in] The key of key-value
 * @return Corresponding element of retrospective lock list
 */
template<typename T>
T *TxExecutor::searchRLL(uint64_t key) {
  // will do : binary search
  for (auto itr = RLL_.begin(); itr != RLL_.end(); ++itr) {
    if ((*itr).key_ == key) return &(*itr);
  }

  return nullptr;
}

/**
 * @brief Remove element from current lock list
 * @param [in] key The kye of key-value.
 * @pre This function is called by handling about MQL lock.
 * This work is not over now.
 * @return void
 */
void TxExecutor::removeFromCLL(uint64_t key) {
  int ctr = 0;
  for (auto itr = CLL_.begin(); itr != CLL_.end(); ++itr) {
    if ((*itr).key_ == key)
      break;
    else
      ctr++;
  }

  CLL_.erase(CLL_.begin() + ctr);
}

/**
 * @brief initialize function of transaction.
 * @return void
 */
void TxExecutor::begin() {
  /**
   * Init by TransactionStatus::inFlight status.
   */
  this->status_ = TransactionStatus::inFlight;
  this->max_rset_.obj_ = 0;
  this->max_wset_.obj_ = 0;

  return;
}

/**
 * @brief Transaction read function.
 * @param [in] key The key of key-value
 */
void TxExecutor::read(uint64_t key) {
#if ADD_ANALYSIS
  uint64_t start(rdtscp());
#endif

  // Default constructor of these variable cause error (-fpermissive)
  // "crosses initialization of ..."
  // So it locate before first goto instruction.
  Epotemp loadepot;
  Tidword expected, desired;

  /**
   * read-own-writes or re-read from local read set.
   */
  if (searchWriteSet(key)) goto FINISH_READ;
  if (searchReadSet(key)) goto FINISH_READ;

  /**
   * Search record from data structure.
   */
  Tuple *tuple;
#if MASSTREE_USE
  tuple = MT.get_value(key);
#if ADD_ANALYSIS
  ++mres_->local_tree_traversal_;
#endif
#else
  tuple = get_tuple(Table, key);
#endif

  // tuple doesn't exist in read/write set.
#ifdef RWLOCK
  LockElement<RWLock> *inRLL;
  inRLL = searchRLL<LockElement<RWLock>>(key);
#endif  // RWLOCK
#ifdef MQLOCK
  LockElement<MQLock> *inRLL;
  inRLL = searchRLL<LockElement<MQLock>>(key);
#endif  // MQLOCK

  /**
   * Check corresponding temperature.
   */
  size_t epotemp_index;
  epotemp_index = key * sizeof(Tuple) / FLAGS_per_xx_temp;
  loadepot.obj_ = loadAcquire(EpotempAry[epotemp_index].obj_);
  bool needVerification;
  needVerification = true;

  if (inRLL != nullptr) {
    /**
     * This transaction is after abort.
     */
    lock(key, tuple, inRLL->mode_);
    if (this->status_ == TransactionStatus::aborted) {
      /**
       * Fail to acqiure lock.
       */
      goto FINISH_READ;
    } else {
      needVerification = false;
      /**
       * Because it could acquire lock.
       */
    }
  } else if (loadepot.temp >= TEMP_THRESHOLD) {
    /**
     * This transaction is not after abort, 
     * however, it accesses high temperature record.
     */
    // printf("key:\t%lu, temp:\t%lu\n", key, loadepot.temp_);
    lock(key, tuple, false);
    if (this->status_ == TransactionStatus::aborted) {
      /**
       * Fail to acqiure lock.
       */
      goto FINISH_READ;
    } else {
      needVerification = false;
      /**
       * Because it could acquire lock.
       */
    }
  }

  if (needVerification) {
    /**
     * This transaction accesses low temperature record, so takes occ approach.
     */
    expected.obj_ = __atomic_load_n(&(tuple->tidword_.obj_), __ATOMIC_ACQUIRE);
    for (;;) {
#ifdef RWLOCK
      while (tuple->rwlock_.ldAcqCounter() == W_LOCKED) {
#endif  // RWLOCK
      /* if you wait due to being write-locked, it may occur dead lock.
      // it need to guarantee that this parts definitely progress.
      // So it sholud wait expected.lock because it will be released
      definitely.
      //
      // if expected.lock raise, opponent worker entered pre-commit phase.
      expected.obj_ = __atomic_load_n(&(tuple->tidword_.obj_),
      __ATOMIC_ACQUIRE);*/

      if (key < CLL_.back().key_) {
        status_ = TransactionStatus::aborted;
        read_set_.emplace_back(key, tuple);
        goto FINISH_READ;
      } else {
        expected.obj_ =
                __atomic_load_n(&(tuple->tidword_.obj_), __ATOMIC_ACQUIRE);
      }
    }

    memcpy(return_val_, tuple->val_, VAL_SIZE);  // read

    desired.obj_ = __atomic_load_n(&(tuple->tidword_.obj_), __ATOMIC_ACQUIRE);
    if (expected == desired)
      break;
    else
      expected.obj_ = desired.obj_;
  }
}

else {
// it already got read-lock.
// So it can load payload atomically by one loading tidword.
expected.
obj_ = __atomic_load_n(&(tuple->tidword_.obj_), __ATOMIC_ACQUIRE);
memcpy(return_val_, tuple
->val_, VAL_SIZE);  // read
}

read_set_.
emplace_back(expected, key, tuple, return_val_
);

FINISH_READ:
#if ADD_ANALYSIS
mres_->local_read_latency_ += rdtscp() - start;
#endif
return;
}

/**
 * @brief Transaction write function.
 * @param [in] key The key of key-value
 */
void TxExecutor::write(uint64_t key) {
#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif

  // these variable cause error (-fpermissive)
  // "crosses initialization of ..."
  // So it locate before first goto instruction.
  Epotemp loadepot;

  // tuple exists in write set.
  if (searchWriteSet(key)) goto FINISH_WRITE;

  Tuple *tuple;
  ReadElement<Tuple> *re;
  re = searchReadSet(key);
  if (re) {
    /**
     * If it can find record in read set, use this for high performance.
     */
    tuple = re->rcdptr_;
  } else {
    /**
     * Search record from data structure.
     */
#if MASSTREE_USE
    tuple = MT.get_value(key);
#if ADD_ANALYSIS
    ++mres_->local_tree_traversal_;
#endif
#else
    tuple = get_tuple(Table, key);
#endif
  }

  /**
   * Check corresponding temperature.
   */
  size_t epotemp_index;
  epotemp_index = key * sizeof(Tuple) / FLAGS_per_xx_temp;
  loadepot.obj_ = loadAcquire(EpotempAry[epotemp_index].obj_);

  /**
   * If this record has high temperature, use lock.
   */
  if (loadepot.temp >= TEMP_THRESHOLD) lock(key, tuple, true);
  /**
   * If it failed locking, it aborts.
   */
  if (this->status_ == TransactionStatus::aborted) goto FINISH_WRITE;

#ifdef RWLOCK
  LockElement<RWLock> *inRLL;
  inRLL = searchRLL<LockElement<RWLock>>(key);
#endif  // RWLOCK
#ifdef MQLOCK
  LockElement<MQLock> *inRLL = searchRLL<LockElement<MQLock>>(key);
#endif  // MQLOCK
  if (inRLL != nullptr) lock(key, tuple, true);
  if (this->status_ == TransactionStatus::aborted) goto FINISH_WRITE;

  // write_set_.emplace_back(key, write_val_);
  write_set_.emplace_back(key, tuple);

FINISH_WRITE:
#if ADD_ANALYSIS
  mres_->local_write_latency_ += rdtscp() - start;
#endif
  return;
}

void TxExecutor::read_write(uint64_t key) {
  // Default constructor of these variable cause error (-fpermissive)
  // "crosses initialization of ..."
  // So it locate before first goto instruction.
  Epotemp loadepot;
  Tidword expected, desired;

  Tuple *tuple;
  if (searchWriteSet(key)) goto FINISH_ALL;
  ReadElement<Tuple> *re;
  re = searchReadSet(key);
  if (re) {
    tuple = re->rcdptr_;
    goto FINISH_READ;
  } else {
    /**
     * Search record from data structure.
     */
#if MASSTREE_USE
    tuple = MT.get_value(key);
#if ADD_ANALYSIS
    ++mres_->local_tree_traversal_;
#endif
#else
    tuple = get_tuple(Table, key);
#endif
  }

  // tuple doesn't exist in read/write set.
#ifdef RWLOCK
  LockElement<RWLock> *inRLL;
  inRLL = searchRLL<LockElement<RWLock>>(key);
#endif  // RWLOCK
#ifdef MQLOCK
  LockElement<MQLock> *inRLL;
  inRLL = searchRLL<LockElement<MQLock>>(key);
#endif  // MQLOCK

  /**
   * Check corresponding temperature.
   */
  size_t epotemp_index;
  epotemp_index = key * sizeof(Tuple) / FLAGS_per_xx_temp;
  loadepot.obj_ = loadAcquire(EpotempAry[epotemp_index].obj_);
  bool needVerification;
  needVerification = true;

  if (inRLL != nullptr) {
    lock(key, tuple, inRLL->mode_);
    if (this->status_ == TransactionStatus::aborted) {
      goto FINISH_ALL;
    } else {
      needVerification = false;
    }
  } else if (loadepot.temp >= TEMP_THRESHOLD) {
    // printf("key:\t%lu, temp:\t%lu\n", key, loadepot.temp_);
    // this lock for write, so write-mode.
    lock(key, tuple, true);
    if (this->status_ == TransactionStatus::aborted) {
      goto FINISH_ALL;
    } else {
      needVerification = false;
    }
  }

  if (needVerification) {
    expected.obj_ = __atomic_load_n(&(tuple->tidword_.obj_), __ATOMIC_ACQUIRE);
    for (;;) {
#ifdef RWLOCK
      while (tuple->rwlock_.ldAcqCounter() == W_LOCKED) {
#endif  // RWLOCK
      /* if you wait due to being write-locked, it may occur dead lock.
      // it need to guarantee that this parts definitely progress.
      // So it sholud wait expected.lock because it will be released
      definitely.
      //
      // if expected.lock raise, opponent worker entered pre-commit phase.
      expected.obj_ = __atomic_load_n(&(tuple->tidword_.obj_),
      __ATOMIC_ACQUIRE);*/

      if (key < CLL_.back().key_) {
        status_ = TransactionStatus::aborted;
        read_set_.emplace_back(key, tuple);
        goto FINISH_READ;
      } else {
        expected.obj_ =
                __atomic_load_n(&(tuple->tidword_.obj_), __ATOMIC_ACQUIRE);
      }
    }

    memcpy(return_val_, tuple->val_, VAL_SIZE);  // read

    desired.obj_ = __atomic_load_n(&(tuple->tidword_.obj_), __ATOMIC_ACQUIRE);
    if (expected == desired)
      break;
    else
      expected.obj_ = desired.obj_;
  }
}

else {
// it already got read-lock.
expected.
obj_ = __atomic_load_n(&(tuple->tidword_.obj_), __ATOMIC_ACQUIRE);
memcpy(return_val_, tuple
->val_, VAL_SIZE);  // read
}

read_set_.
emplace_back(expected, key, tuple, return_val_
);

FINISH_READ:

write_set_.
emplace_back(key, tuple
);

FINISH_ALL:

return;
}

void TxExecutor::lock(uint64_t key, Tuple *tuple, bool mode) {
  unsigned int vioctr = 0;
  unsigned int threshold;
  bool upgrade = false;

#ifdef RWLOCK
  LockElement<RWLock> *le = nullptr;
#endif  // RWLOCK
  // RWLOCK : アップグレードするとき，CLL_ ループで該当する
  // エレメントを記憶しておき，そのエレメントを更新するため．
  // MQLOCK : アップグレード機能が無いので，不要．
  // reader ロックを解放して，CLL_ から除去して，writer ロックをかける．

  // lock exists in CLL_ (current lock list)
  sort(CLL_.begin(), CLL_.end());
  for (auto itr = CLL_.begin(); itr != CLL_.end(); ++itr) {
    // lock already exists in CLL_
    //    && its lock mode is equal to needed mode or it is stronger than needed
    //    mode.
    if ((*itr).key_ == key) {
      if (mode == (*itr).mode_ || mode < (*itr).mode_)
        return;
      else {
#ifdef RWLOCK
        le = &(*itr);
#endif  // RWLOCK
        upgrade = true;
      }
    }

    // collect violation
    if ((*itr).key_ >= key) {
      if (vioctr == 0) threshold = (*itr).key_;

      vioctr++;
    }
  }

  if (vioctr == 0) threshold = -1;

  // if too many violations
  // i set my condition of too many because the original paper of mocc didn't
  // show the condition of too many.
  // if ((CLL_.size() / 2) < vioctr && CLL_.size() >= (MAX_OPE / 2)) {
  // test condition. mustn't enter.
  if ((vioctr > 100)) {
#ifdef RWLOCK
    if (mode) {
      if (upgrade) {
        if (tuple->rwlock_.upgrade()) {
          le->mode_ = true;
          return;
        } else {
          this->status_ = TransactionStatus::aborted;
          return;
        }
      } else if (tuple->rwlock_.w_trylock()) {
        CLL_.push_back(LockElement<RWLock>(key, &(tuple->rwlock_), true));
        return;
      } else {
        this->status_ = TransactionStatus::aborted;
        return;
      }
    } else {
      if (tuple->rwlock_.r_trylock()) {
        CLL_.push_back(LockElement<RWLock>(key, &(tuple->rwlock_), false));
        return;
      } else {
        this->status_ = TransactionStatus::aborted;
        return;
      }
    }
#endif  // RWLOCK
#ifdef MQLOCK
    if (mode) {
      if (upgrade) {
        tuple->mqlock.release_reader_lock(this->locknum, key);
        removeFromCLL(key);
        if (tuple->mqlock.acquire_writer_lock(this->locknum, key, true) ==
            MQL_RESULT::Acquired) {
          CLL_.push_back(LockElement<MQLock>(key, &(tuple->mqlock), true));
          return;
        } else {
          this->status = TransactionStatus::aborted;
          return;
        }
      } else if (tuple->mqlock.acquire_writer_lock(this->locknum, key, true) ==
                 MQL_RESULT::Acquired) {
        CLL_.push_back(LockElement<MQLock>(key, &(tuple->mqlock), true));
        return;
      } else {
        this->status = TransactionStatus::aborted;
        return;
      }
    } else {
      if (tuple->mqlock.acquire_reader_lock(this->locknum, key, true) ==
          MQL_RESULT::Acquired) {
        CLL_.push_back(LockElement<MQLock>(key, &(tuple->mqlock), false));
        return;
      } else {
        this->status = TransactionStatus::aborted;
        return;
      }
    }
#endif  // MQLOCK
  }

  if (vioctr != 0) {
    // not in canonical mode. restore.
    for (auto itr = CLL_.begin() + (CLL_.size() - vioctr); itr != CLL_.end();
         ++itr) {
#ifdef RWLOCK
      if ((*itr).mode_)
        (*itr).lock_->w_unlock();
      else
        (*itr).lock_->r_unlock();
#endif  // RWLOCK

#ifdef MQLOCK
      if ((*itr).mode_)
        (*itr).lock_->release_writer_lock(this->locknum, key);
      else
        (*itr).lock_->release_reader_lock(this->locknum, key);
#endif  // MQLOCK
    }

    // delete from CLL_
    if (CLL_.size() == vioctr)
      CLL_.clear();
    else
      CLL_.erase(CLL_.begin() + (CLL_.size() - vioctr), CLL_.end());
  }

  // unconditional lock in canonical mode.
  for (auto itr = RLL_.begin(); itr != RLL_.end(); ++itr) {
    if ((*itr).key_ <= threshold) continue;
    if ((*itr).key_ < key) {
#ifdef RWLOCK
      if ((*itr).mode_)
        (*itr).lock_->w_lock();
      else
        (*itr).lock_->r_lock();
#endif  // RWLOCK

#ifdef MQLOCK
      if ((*itr).mode_)
        (*itr).lock_->acquire_writer_lock(this->locknum, key);
      else
        (*itr).lock_->acquire_reader_lock(this->locknum, key);
#endif  // MQLOCK

      CLL_.emplace_back((*itr).key_, (*itr).lock_, (*itr).mode_);
    } else
      break;
  }

#ifdef RWLOCK
  if (mode)
    tuple->rwlock_.w_lock();
  else
    tuple->rwlock_.r_lock();
  CLL_.emplace_back(key, &(tuple->rwlock_), mode);
  return;
#endif  // RWLOCK

#ifdef MQLOCK
  if (mode)
    tuple->mqlock.acquire_writer_lock(this->locknum, key, false);
  else
    tuple->mqlock.acquire_reader_lock(this->locknum, key, false);
  CLL_.push_back(LockElement<MQLock>(key, tuple->mqlock, mode));
  return;
#endif  // MQLOCK
}

void TxExecutor::construct_RLL() {
  RLL_.clear();

  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
#ifdef RWLOCK
    RLL_.emplace_back((*itr).key_, &((*itr).rcdptr_->rwlock_), true);
#endif  // RWLOCK
#ifdef MQLOCK
    RLL_.push_back(
        LockElement<MQLock>((*itr).key_, &(Table[(*itr).key_].mqlock), true));
#endif  // MQLOCK
  }

  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
    // maintain temprature p
    size_t epotemp_index = (*itr).key_ * sizeof(Tuple) / FLAGS_per_xx_temp;
    if ((*itr).failed_verification_) {
      /**
       * This transaction aborted by this operation.
       * So our custom optimization does temperature management targeting this record.
       */
      Epotemp expected, desired;
      expected.obj_ = loadAcquire(EpotempAry[epotemp_index].obj_);

#if TEMPERATURE_RESET_OPT
      uint64_t nowepo;
      nowepo = (loadAcquireGE()).obj_;
      if (expected.epoch != nowepo) {
        desired.epoch = nowepo;
        desired.temp = 0;
        storeRelease(EpotempAry[epotemp_index].obj_, desired.obj_);
#if ADD_ANALYSIS
        ++mres_->local_temperature_resets_;
#endif
      }
#endif

      for (;;) {
        // printf("key:\t%lu,\ttemp_index:\t%zu,\ttemp:\t%lu\n", (*itr).key_,
        // epotemp_index, expected.temp_);
        if (expected.temp == TEMP_MAX) {
          break;
        } else if (rnd_->next() % (1 << expected.temp) == 0) {
          desired = expected;
          desired.temp = expected.temp + 1;
        } else {
          break;
        }

        if (compareExchange(EpotempAry[epotemp_index].obj_, expected.obj_,
                            desired.obj_))
          break;
      }
    }

    // check whether itr exists in RLL_
#ifdef RWLOCK
    if (searchRLL<LockElement<RWLock>>((*itr).key_) != nullptr) continue;
#endif  // RWLOCK
#ifdef MQLOCK
    if (searchRLL<LockElement<MQLock>>((*itr).key_) != nullptr) continue;
#endif  // MQLOCK

    // r not in RLL_
    // if temprature >= threshold
    //  || r failed verification
    Epotemp loadepot;
    loadepot.obj_ = loadAcquire(EpotempAry[epotemp_index].obj_);
    if (loadepot.temp >= TEMP_THRESHOLD || (*itr).failed_verification_) {
#ifdef RWLOCK
      RLL_.emplace_back((*itr).key_, &((*itr).rcdptr_->rwlock_), false);
#endif  // RWLOCK
#ifdef MQLOCK
      RLL_.push_back(
          LockElement<MQLock>((*itr).key_, &((*itr).rcdptr_->mqlock_), false));
#endif  // MQLOCK
    }
  }

  sort(RLL_.begin(), RLL_.end());

  return;
}

bool TxExecutor::commit() {
  Tidword expected, desired;

  // phase 1 lock write set.
  sort(write_set_.begin(), write_set_.end());
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    lock((*itr).key_, (*itr).rcdptr_, true);
    if (this->status_ == TransactionStatus::aborted) {
      return false;
    }

    this->max_wset_ = max(this->max_wset_, (*itr).rcdptr_->tidword_);
  }

  asm volatile("":: : "memory");
  __atomic_store_n(&(ThLocalEpoch[thid_].obj_), (loadAcquireGE()).obj_,
                   __ATOMIC_RELEASE);
  asm volatile("":: : "memory");

  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
    Tidword check;
    check.obj_ =
            __atomic_load_n(&((*itr).rcdptr_->tidword_.obj_), __ATOMIC_ACQUIRE);
    if ((*itr).tidword_.epoch != check.epoch ||
        (*itr).tidword_.tid != check.tid) {
      (*itr).failed_verification_ = true;
      this->status_ = TransactionStatus::aborted;
#if ADD_ANALYSIS
      ++mres_->local_validation_failure_by_tid_;
#endif
      return false;
    }

    // Silo protocol
#ifdef RWLOCK
    if ((*itr).rcdptr_->rwlock_.ldAcqCounter() == W_LOCKED &&
        searchWriteSet((*itr).key_) == nullptr) {
#endif  // RWLOCK
    // if the rwlock is already acquired and the owner isn't me, abort.
    (*itr).failed_verification_ = true;
    this->status_ = TransactionStatus::aborted;
#if ADD_ANALYSIS
    ++mres_->local_validation_failure_by_writelock_;
#endif
    return false;
  }

  this->max_rset_ = max(this->max_rset_, (*itr).rcdptr_->tidword_);
}

return true;
}

/**
 * @brief function about abort.
 * Clean-up local read/write set.
 * Release lock.
 * @return void
 */
void TxExecutor::abort() {
  // unlock CLL_
  unlockCLL();

  construct_RLL();

  read_set_.clear();
  write_set_.clear();

  ++mres_->local_abort_counts_;

#if BACK_OFF
#if ADD_ANALYSIS
  uint64_t start(rdtscp());
#endif

  Backoff::backoff(FLAGS_clocks_per_us);

#if ADD_ANALYSIS
  mres_->local_backoff_latency_ += rdtscp() - start;
#endif
#endif

  return;
}

void TxExecutor::unlockCLL() {
  Tidword expected, desired;

  for (auto itr = CLL_.begin(); itr != CLL_.end(); ++itr) {
#ifdef RWLOCK
    if ((*itr).mode_)
      (*itr).lock_->w_unlock();
    else
      (*itr).lock_->r_unlock();
#endif  // RWLOCK

#ifdef MQLOCK
    if ((*itr).mode_) {
      (*itr).lock_->release_writer_lock(this->locknum, (*itr).key_);
    } else
      (*itr).lock_->release_reader_lock(this->locknum, (*itr).key_);
#endif
  }
  CLL_.clear();
}

void TxExecutor::writePhase() {
  Tidword tid_a, tid_b, tid_c;

  // calculates (a)
  tid_a = max(this->max_rset_, this->max_wset_);
  tid_a.tid++;

  // calculates (b)
  // larger than the worker's most recently chosen TID,
  tid_b = mrctid_;
  tid_b.tid++;

  // calculates (c)
  tid_c.epoch = ThLocalEpoch[thid_].obj_;

  // compare a, b, c
  Tidword maxtid = max({tid_a, tid_b, tid_c});
  mrctid_ = maxtid;

  // write (record, commit-tid)
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    // update and down lockBit
    memcpy((*itr).rcdptr_->val_, write_val_, VAL_SIZE);
    __atomic_store_n(&((*itr).rcdptr_->tidword_.obj_), maxtid.obj_,
                     __ATOMIC_RELEASE);
  }

  unlockCLL();
  RLL_.clear();
  read_set_.clear();
  write_set_.clear();
}

void TxExecutor::dispCLL() {
  cout << "th " << this->thid_ << ": CLL_: ";
  for (auto itr = CLL_.begin(); itr != CLL_.end(); ++itr) {
    cout << (*itr).key_ << "(";
    if ((*itr).mode_)
      cout << "w) ";
    else
      cout << "r) ";
  }
  cout << endl;
}

void TxExecutor::dispRLL() {
  cout << "th " << this->thid_ << ": RLL_: ";
  for (auto itr = RLL_.begin(); itr != RLL_.end(); ++itr) {
    cout << (*itr).key_ << "(";
    if ((*itr).mode_)
      cout << "w) ";
    else
      cout << "r) ";
  }
  cout << endl;
}

void TxExecutor::dispWS() {
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    cout << "(" << (*itr).key_ << ", " << (*itr).rcdptr_ << ")" << endl;
  }
  cout << endl;
}
