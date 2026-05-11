#include <stdio.h>
#include <algorithm>
#include <cmath>
#include <thread>

#include "../include/atomic_wrapper.hh"
#include "../include/backoff.hh"
#include "include/atomic_tool.hh"
#include "include/scan_callback.hh"
#include "include/transaction.hh"
#include "include/tuple.hh"

using namespace std;

extern std::vector<Result> MoccResult;
extern void moccLeaderWork(uint64_t &epoch_timer_start, uint64_t &epoch_timer_stop);

/**
 * @brief Search xxx set
 * @detail Search element of local set corresponding to given key.
 * In this prototype system, the value to be updated for each worker thread 
 * is fixed for high performance, so it is only necessary to check the key match.
 * @param Key [in] the key of key-value
 * @return Corresponding element of local set
 */
ReadElement<Tuple> *TxExecutor::searchReadSet(Storage s, std::string_view key) {
  for (auto &re : read_set_) {
    if (re.storage_ != s) continue;
    if (re.key_ == key) return &re;
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
WriteElement<Tuple> *TxExecutor::searchWriteSet(Storage s, std::string_view key) {
  for (auto &we : write_set_) {
    if (we.storage_ != s) continue;
    if (we.key_ == key) return &we;
  }

  return nullptr;
}

/**
 * @brief Search element from retrospective lock list.
 * @param key [in] The key of key-value
 * @return Corresponding element of retrospective lock list
 */
template<typename T>
T *TxExecutor::searchRLL(Tuple* key) {
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
void TxExecutor::removeFromCLL(Tuple* key) {
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
   * Init by TransactionStatus::inflight status.
   */
  this->status_ = TransactionStatus::inflight;
  this->max_rset_.obj_ = 0;
  this->max_wset_.obj_ = 0;

  return;
}

/**
 * @brief Transaction read function.
 * @param [in] key The key of key-value
 */
Status TxExecutor::read(Storage s, std::string_view key, TupleBody** body) {
#if ADD_ANALYSIS
  uint64_t start(rdtscp());
#endif

  // Default constructor of these variable cause error (-fpermissive)
  // "crosses initialization of ..."
  // So it locate before first goto instruction.
  Epotemp loadepot;
  Tidword expected, desired;
  TupleBody b;
  ReadElement<Tuple>* re;
  WriteElement<Tuple>* we;
  Status stat;

  /**
   * read-own-writes or re-read from local read set.
   */
  re = searchReadSet(s, key);
  if (re) {
    *body = &(re->body_);
    goto FINISH_READ;
  }
  we = searchWriteSet(s, key);
  if (we) {
    *body = &(we->body_);
    goto FINISH_READ;
  }

  /**
   * Search record from data structure.
   */
  Tuple *tuple;
  tuple = Masstrees[get_storage(s)].get_value(key);
#if ADD_ANALYSIS
  ++result_->local_tree_traversal_;
#endif
  if (tuple == nullptr) return Status::WARN_NOT_FOUND;

  stat = read_internal(s, key, tuple);
  if (stat != Status::OK) return stat;
  *body = &(read_set_.back().body_);

FINISH_READ:
#if ADD_ANALYSIS
  result_->local_read_latency_ += rdtscp() - start;
#endif
  return Status::OK;
}

Status TxExecutor::read_internal(Storage s, std::string_view key, Tuple* tuple) {
  // Default constructor of these variable cause error (-fpermissive)
  // "crosses initialization of ..."
  // So it locate before first goto instruction.
  Epotemp loadepot;
  Tidword expected, desired;
  TupleBody b;

  // tuple doesn't exist in read/write set.
#ifdef RWLOCK
  LockElement<ReaderWriterLock> *inRLL;
  inRLL = searchRLL<LockElement<ReaderWriterLock>>(tuple);
#endif  // RWLOCK
#ifdef MQLOCK
  LockElement<MQLock> *inRLL;
  inRLL = searchRLL<LockElement<MQLock>>(tuple);
#endif  // MQLOCK

  /**
   * Check corresponding temperature.
   */
  loadepot.obj_ = loadAcquire(tuple->epotemp_.obj_);
  bool needVerification;
  needVerification = true;

  if (inRLL != nullptr) {
    /**
     * This transaction is after abort.
     */
    lock(tuple, inRLL->mode_);
    if (this->status_ == TransactionStatus::aborted) {
      /**
       * Fail to acqiure lock.
       */
      return Status::ERROR_LOCK_FAILED;
    } else {
      needVerification = false;
      /**
       * Because it could acquire lock.
       */
    }
  } else if (loadepot.temp >= FLAGS_temp_threshold) {
    /**
     * This transaction is not after abort, 
     * however, it accesses high temperature record.
     */
    // printf("key:\t%lu, temp:\t%lu\n", key, loadepot.temp_);
    lock(tuple, false);
    if (this->status_ == TransactionStatus::aborted) {
      /**
       * Fail to acqiure lock.
       */
      return Status::ERROR_LOCK_FAILED;
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
      while (tuple->rwlock_.ldAcqCounter() == W_LOCKED) {
        /* if you wait due to being write-locked, it may occur dead lock.
        // it need to guarantee that this parts definitely progress.
        // So it sholud wait expected.lock because it will be released
        definitely.
        //
        // if expected.lock raise, opponent worker entered pre-commit phase.
        expected.obj_ = __atomic_load_n(&(tuple->tidword_.obj_),
        __ATOMIC_ACQUIRE);*/
        if (CLL_.size() > 0 && tuple < CLL_.back().key_) {
          status_ = TransactionStatus::aborted;
          read_set_.emplace_back(key, tuple);
          return Status::ERROR_LOCK_FAILED;
        } else {
          expected.obj_ =
                  __atomic_load_n(&(tuple->tidword_.obj_), __ATOMIC_ACQUIRE);
        }
      }

      if (expected.absent) {
        status_ = TransactionStatus::aborted;
        return Status::WARN_NOT_FOUND;
      }

      // read
      b = TupleBody(tuple->body_.get_key(), tuple->body_.get_val(), tuple->body_.get_val_align());

      desired.obj_ = __atomic_load_n(&(tuple->tidword_.obj_), __ATOMIC_ACQUIRE);
      if (expected == desired)
        break;
      else
        expected.obj_ = desired.obj_;
    }
  } else {
    // it already got read-lock.
    // So it can load payload atomically by one loading tidword.
    expected.obj_ = __atomic_load_n(&(tuple->tidword_.obj_), __ATOMIC_ACQUIRE);
    // read
    b = TupleBody(tuple->body_.get_key(), tuple->body_.get_val(), tuple->body_.get_val_align());
  }
  read_set_.emplace_back(s, key, tuple, std::move(b), expected);

  return Status::OK;
}

Status TxExecutor::scan(const Storage s,
                        std::string_view left_key, bool l_exclusive,
                        std::string_view right_key, bool r_exclusive,
                        std::vector<TupleBody*>& result) {
  return scan(s, left_key, l_exclusive, right_key, r_exclusive, result, -1);
}

Status TxExecutor::scan(const Storage s,
                      std::string_view left_key, bool l_exclusive,
                      std::string_view right_key, bool r_exclusive,
                      std::vector<TupleBody*>& result, int64_t limit) {
  result.clear();
  auto rset_init_size = read_set_.size();

  std::vector<Tuple*> scan_res;
  Masstrees[get_storage(s)].scan(
            left_key.empty() ? nullptr : left_key.data(), left_key.size(),
            l_exclusive, right_key.empty() ? nullptr : right_key.data(),
            right_key.size(), r_exclusive, &scan_res, limit,
            callback_);

  for (auto &&itr : scan_res) {
    ReadElement<Tuple>* re = searchReadSet(s, itr->body_.get_key());
    if (re) {
      result.emplace_back(&(re->body_));
      continue;
    }

    WriteElement<Tuple>* we = searchWriteSet(s, itr->body_.get_key());
    if (we) {
      result.emplace_back(&(we->body_));
      continue;
    }

    Status stat = read_internal(s, itr->body_.get_key(), itr);
    if (this->status_ == TransactionStatus::aborted) return stat;
  }

  if (rset_init_size != read_set_.size()) {
    for (auto itr = read_set_.begin() + rset_init_size;
         itr != read_set_.end(); ++itr) {
      result.emplace_back(&((*itr).body_));
    }
  }

  return Status::OK;
}

/**
 * @brief Transaction write function.
 * @param [in] key The key of key-value
 */
Status TxExecutor::write(Storage s, std::string_view key, TupleBody&& body) {
#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif

  // these variable cause error (-fpermissive)
  // "crosses initialization of ..."
  // So it locate before first goto instruction.
  Epotemp loadepot;

  // tuple exists in write set.
  if (searchWriteSet(s, key)) goto FINISH_WRITE;

  Tuple *tuple;
  ReadElement<Tuple> *re;
  re = searchReadSet(s, key);
  if (re) {
    /**
     * If it can find record in read set, use this for high performance.
     */
    tuple = re->rcdptr_;
  } else {
    /**
     * Search record from data structure.
     */
    tuple = Masstrees[get_storage(s)].get_value(key);
#if ADD_ANALYSIS
    ++result_->local_tree_traversal_;
#endif
    if (tuple == nullptr) return Status::WARN_NOT_FOUND;
  }

  /**
   * Check corresponding temperature.
   */
  loadepot.obj_ = loadAcquire(tuple->epotemp_.obj_);

  /**
   * If this record has high temperature, use lock.
   */
  if (loadepot.temp >= FLAGS_temp_threshold) lock(tuple, true);
  /**
   * If it failed locking, it aborts.
   */
  if (this->status_ == TransactionStatus::aborted) return Status::WARN_NOT_FOUND;

#ifdef RWLOCK
  LockElement<ReaderWriterLock> *inRLL;
  inRLL = searchRLL<LockElement<ReaderWriterLock>>(tuple);
#endif  // RWLOCK
#ifdef MQLOCK
  LockElement<MQLock> *inRLL = searchRLL<LockElement<MQLock>>(tuple);
#endif  // MQLOCK
  if (inRLL != nullptr) lock(tuple, true);
  if (this->status_ == TransactionStatus::aborted) return Status::WARN_NOT_FOUND;

  write_set_.emplace_back(s, key, tuple, std::move(body), OpType::UPDATE);

FINISH_WRITE:
#if ADD_ANALYSIS
  result_->local_write_latency_ += rdtscp() - start;
#endif
  return Status::OK;
}

Status TxExecutor::insert(Storage s, std::string_view key, TupleBody&& body) {
#if ADD_ANALYSIS
  std::uint64_t start = rdtscp();
#endif

  if (searchWriteSet(s, key)) return Status::WARN_ALREADY_EXISTS;

  Tuple* tuple = Masstrees[get_storage(s)].get_value(key);
#if ADD_ANALYSIS
  ++result_->local_tree_traversal_;
#endif
  if (tuple != nullptr) {
    return Status::WARN_ALREADY_EXISTS;
  }

  tuple = new Tuple();
  tuple->init(std::move(body));

  typename MasstreeWrapper<Tuple>::insert_info_t insert_info;
  Status stat = Masstrees[get_storage(s)].insert_value(key, tuple, &insert_info);
  if (stat == Status::WARN_ALREADY_EXISTS) {
    delete tuple;
    return stat;
  }
  if (insert_info.node) {
    if (!node_map_.empty()) {
      auto it = node_map_.find((void*)insert_info.node);
      if (it != node_map_.end()) {
        if (unlikely(it->second != insert_info.old_version)) {
          status_ = TransactionStatus::aborted;
          return Status::ERROR_CONCURRENT_WRITE_OR_DELETE;
        }
        // otherwise, bump the version
        it->second = insert_info.new_version;
      }
    }
  } else {
    ERR;
  }

  write_set_.emplace_back(s, key, tuple, OpType::INSERT);

#if ADD_ANALYSIS
  result_->local_write_latency_ += rdtscp() - start;
#endif
  return Status::OK;
}

Status TxExecutor::delete_record(Storage s, std::string_view key) {
#if ADD_ANALYSIS
  std::uint64_t start = rdtscp();
#endif
  Epotemp loadepot;

  // cancel previous write
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    if ((*itr).storage_ != s) continue;
    if ((*itr).key_ == key) {
      write_set_.erase(itr);
    }
  }

  Tuple* tuple;
  ReadElement<Tuple>* re;
  re = searchReadSet(s, key);
  if (re) {
    tuple = re->rcdptr_;
  } else {
    tuple = Masstrees[get_storage(s)].get_value(key);
#if ADD_ANALYSIS
  ++result_->local_tree_traversal_;
#endif
    if (tuple == nullptr) return Status::WARN_NOT_FOUND;
  }

  /**
   * Check corresponding temperature.
   */
  loadepot.obj_ = loadAcquire(tuple->epotemp_.obj_);

  /**
   * If this record has high temperature, use lock.
   */
  if (loadepot.temp >= FLAGS_temp_threshold) lock(tuple, true);
  /**
   * If it failed locking, it aborts.
   */
  if (this->status_ == TransactionStatus::aborted)
    return Status::ERROR_CONCURRENT_WRITE_OR_DELETE;

#ifdef RWLOCK
  LockElement<ReaderWriterLock> *inRLL;
  inRLL = searchRLL<LockElement<ReaderWriterLock>>(tuple);
#endif  // RWLOCK
#ifdef MQLOCK
  LockElement<MQLock> *inRLL = searchRLL<LockElement<MQLock>>(tuple);
#endif  // MQLOCK
  if (inRLL != nullptr) lock(tuple, true);
  if (this->status_ == TransactionStatus::aborted)
    return Status::ERROR_CONCURRENT_WRITE_OR_DELETE;

  write_set_.emplace_back(s, key, tuple, OpType::DELETE);

FINISH_DELETE:
#if ADD_ANALYSIS
  result_->local_write_latency_ += rdtscp() - start;
#endif
  return Status::OK;
}

// TODO: necessary?
// void TxExecutor::read_write(uint64_t key) {
//   // Default constructor of these variable cause error (-fpermissive)
//   // "crosses initialization of ..."
//   // So it locate before first goto instruction.
//   Epotemp loadepot;
//   Tidword expected, desired;

//   Tuple *tuple;
//   if (searchWriteSet(key)) goto FINISH_ALL;
//   ReadElement<Tuple> *re;
//   re = searchReadSet(key);
//   if (re) {
//     tuple = re->rcdptr_;
//     goto FINISH_READ;
//   } else {
//     /**
//      * Search record from data structure.
//      */
// #if MASSTREE_USE
//     tuple = MT.get_value(key);
// #if ADD_ANALYSIS
//     ++result_->local_tree_traversal_;
// #endif
// #else
//     tuple = get_tuple(Table, key);
// #endif
//   }

//   // tuple doesn't exist in read/write set.
// #ifdef RWLOCK
//   LockElement<ReaderWriterLock> *inRLL;
//   inRLL = searchRLL<LockElement<ReaderWriterLock>>(key);
// #endif  // RWLOCK
// #ifdef MQLOCK
//   LockElement<MQLock> *inRLL;
//   inRLL = searchRLL<LockElement<MQLock>>(key);
// #endif  // MQLOCK

//   /**
//    * Check corresponding temperature.
//    */
//   size_t epotemp_index;
//   epotemp_index = key * sizeof(Tuple) / FLAGS_per_xx_temp;
//   loadepot.obj_ = loadAcquire(EpotempAry[epotemp_index].obj_);
//   bool needVerification;
//   needVerification = true;

//   if (inRLL != nullptr) {
//     lock(key, tuple, inRLL->mode_);
//     if (this->status_ == TransactionStatus::aborted) {
//       goto FINISH_ALL;
//     } else {
//       needVerification = false;
//     }
//   } else if (loadepot.temp >= FLAGS_temp_threshold) {
//     // printf("key:\t%lu, temp:\t%lu\n", key, loadepot.temp_);
//     // this lock for write, so write-mode.
//     lock(key, tuple, true);
//     if (this->status_ == TransactionStatus::aborted) {
//       goto FINISH_ALL;
//     } else {
//       needVerification = false;
//     }
//   }

//   if (needVerification) {
//     expected.obj_ = __atomic_load_n(&(tuple->tidword_.obj_), __ATOMIC_ACQUIRE);
//     for (;;) {
// #ifdef RWLOCK
//       while (tuple->rwlock_.ldAcqCounter() == W_LOCKED) {
// #endif  // RWLOCK
//       /* if you wait due to being write-locked, it may occur dead lock.
//       // it need to guarantee that this parts definitely progress.
//       // So it sholud wait expected.lock because it will be released
//       definitely.
//       //
//       // if expected.lock raise, opponent worker entered pre-commit phase.
//       expected.obj_ = __atomic_load_n(&(tuple->tidword_.obj_),
//       __ATOMIC_ACQUIRE);*/

//       if (key < CLL_.back().key_) {
//         status_ = TransactionStatus::aborted;
//         read_set_.emplace_back(key, tuple);
//         goto FINISH_READ;
//       } else {
//         expected.obj_ =
//                 __atomic_load_n(&(tuple->tidword_.obj_), __ATOMIC_ACQUIRE);
//       }
//     }

//     memcpy(return_val_, tuple->val_, VAL_SIZE);  // read

//     desired.obj_ = __atomic_load_n(&(tuple->tidword_.obj_), __ATOMIC_ACQUIRE);
//     if (expected == desired)
//       break;
//     else
//       expected.obj_ = desired.obj_;
//   }
// }

// else {
// // it already got read-lock.
// expected.
// obj_ = __atomic_load_n(&(tuple->tidword_.obj_), __ATOMIC_ACQUIRE);
// memcpy(return_val_, tuple
// ->val_, VAL_SIZE);  // read
// }

// read_set_.
// emplace_back(expected, key, tuple, return_val_
// );

// FINISH_READ:

// #ifdef INSERT_READ_DELAY_MS
// sleepMs(INSERT_READ_DELAY_MS);
// #endif

// write_set_.
// emplace_back(key, tuple
// );

// FINISH_ALL:

// return;
// }

void TxExecutor::lock(Tuple *tuple, bool mode) {
  unsigned int vioctr = 0;
  Tuple* threshold;
  bool upgrade = false;

#ifdef RWLOCK
  LockElement<ReaderWriterLock> *le = nullptr;
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
    if ((*itr).key_ == tuple) {
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
    if ((*itr).key_ >= tuple) {
      if (vioctr == 0) threshold = (*itr).key_;

      vioctr++;
    }
  }

  if (vioctr == 0) threshold = (Tuple*)-1; // max pointer address

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
        CLL_.push_back(LockElement<ReaderWriterLock>(tuple, &(tuple->rwlock_), true));
        return;
      } else {
        this->status_ = TransactionStatus::aborted;
        return;
      }
    } else {
      if (tuple->rwlock_.r_trylock()) {
        CLL_.push_back(LockElement<ReaderWriterLock>(tuple, &(tuple->rwlock_), false));
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
        tuple->mqlock.release_reader_lock(this->locknum, tuple);
        removeFromCLL(tuple);
        if (tuple->mqlock.acquire_writer_lock(this->locknum, tuple, true) ==
            MQL_RESULT::Acquired) {
          CLL_.push_back(LockElement<MQLock>(tuple, &(tuple->mqlock), true));
          return;
        } else {
          this->status = TransactionStatus::aborted;
          return;
        }
      } else if (tuple->mqlock.acquire_writer_lock(this->locknum, tuple, true) ==
                 MQL_RESULT::Acquired) {
        CLL_.push_back(LockElement<MQLock>(tuple, &(tuple->mqlock), true));
        return;
      } else {
        this->status = TransactionStatus::aborted;
        return;
      }
    } else {
      if (tuple->mqlock.acquire_reader_lock(this->locknum, tuple, true) ==
          MQL_RESULT::Acquired) {
        CLL_.push_back(LockElement<MQLock>(tuple, &(tuple->mqlock), false));
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
        (*itr).lock_->release_writer_lock(this->locknum, tuple);
      else
        (*itr).lock_->release_reader_lock(this->locknum, tuple);
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
    if ((*itr).key_ < tuple) {
#ifdef RWLOCK
      if ((*itr).mode_)
        (*itr).lock_->w_lock();
      else
        (*itr).lock_->r_lock();
#endif  // RWLOCK

#ifdef MQLOCK
      if ((*itr).mode_)
        (*itr).lock_->acquire_writer_lock(this->locknum, tuple);
      else
        (*itr).lock_->acquire_reader_lock(this->locknum, tuple);
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
  CLL_.emplace_back(tuple, &(tuple->rwlock_), mode);
  return;
#endif  // RWLOCK

#ifdef MQLOCK
  if (mode)
    tuple->mqlock.acquire_writer_lock(this->locknum, tuple, false);
  else
    tuple->mqlock.acquire_reader_lock(this->locknum, tuple, false);
  CLL_.push_back(LockElement<MQLock>(tuple, tuple->mqlock, mode));
  return;
#endif  // MQLOCK
}

void TxExecutor::construct_RLL() {
  RLL_.clear();

  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
#ifdef RWLOCK
    RLL_.emplace_back((*itr).rcdptr_, &((*itr).rcdptr_->rwlock_), true);
#endif  // RWLOCK
#ifdef MQLOCK
    RLL_.push_back(
        LockElement<MQLock>((*itr).rcdptr_, &(Table[(*itr).key_].mqlock), true));
#endif  // MQLOCK
  }

  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
    // maintain temprature p
    if ((*itr).failed_verification_) {
      /**
       * This transaction aborted by this operation.
       * So our custom optimization does temperature management targeting this record.
       */
      Epotemp expected, desired;
      expected.obj_ = loadAcquire((*itr).rcdptr_->epotemp_.obj_);

#if TEMPERATURE_RESET_OPT
      uint64_t nowepo;
      nowepo = (loadAcquireGE()).obj_;
      if (expected.epoch != nowepo) {
        desired.epoch = nowepo;
        desired.temp = 0;
        storeRelease((*itr).rcdptr_->epotemp_.obj_, desired.obj_);
#if ADD_ANALYSIS
        ++result_->local_temperature_resets_;
#endif
      }
#endif

      for (;;) {
        // printf("key:\t%lu,\ttemp_index:\t%zu,\ttemp:\t%lu\n", (*itr).key_,
        // epotemp_index, expected.temp_);
        if (expected.temp == TEMP_MAX) {
          break;
        } else if (rnd_.next() % (1 << expected.temp) == 0) {
          desired = expected;
          desired.temp = expected.temp + 1;
        } else {
          break;
        }

        if (compareExchange((*itr).rcdptr_->epotemp_.obj_, expected.obj_,
                            desired.obj_))
          break;
      }
    }

    // check whether itr exists in RLL_
#ifdef RWLOCK
    if (searchRLL<LockElement<ReaderWriterLock>>((*itr).rcdptr_) != nullptr) continue;
#endif  // RWLOCK
#ifdef MQLOCK
    if (searchRLL<LockElement<MQLock>>((*itr).rcdptr_) != nullptr) continue;
#endif  // MQLOCK

    // r not in RLL_
    // if temprature >= threshold
    //  || r failed verification
    Epotemp loadepot;
    loadepot.obj_ = loadAcquire((*itr).rcdptr_->epotemp_.obj_);
    if (loadepot.temp >= FLAGS_temp_threshold || (*itr).failed_verification_) {
#ifdef RWLOCK
      RLL_.emplace_back((*itr).rcdptr_, &((*itr).rcdptr_->rwlock_), false);
#endif  // RWLOCK
#ifdef MQLOCK
      RLL_.push_back(
          LockElement<MQLock>((*itr).rcdptr_, &((*itr).rcdptr_->mqlock_), false));
#endif  // MQLOCK
    }
  }

  sort(RLL_.begin(), RLL_.end());

  return;
}

bool TxExecutor::validation() {
  Tidword expected, desired;

  // phase 1 lock write set.
  sort(write_set_.begin(), write_set_.end());
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    if (itr->op_ == OpType::INSERT) continue;
    lock((*itr).rcdptr_, true);
    if (this->status_ == TransactionStatus::aborted
      || (itr->op_ == OpType::UPDATE && itr->rcdptr_->tidword_.absent)) {
      this->status_ = TransactionStatus::aborted;
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
      ++result_->local_validation_failure_by_tid_;
#endif
      return false;
    }

    // Silo protocol
#ifdef RWLOCK
    if ((*itr).rcdptr_->rwlock_.ldAcqCounter() == W_LOCKED &&
        searchWriteSet((*itr).storage_, (*itr).key_) == nullptr) {
#endif  // RWLOCK
    // if the rwlock is already acquired and the owner isn't me, abort.
    (*itr).failed_verification_ = true;
    this->status_ = TransactionStatus::aborted;
#if ADD_ANALYSIS
    ++result_->local_validation_failure_by_writelock_;
#endif
    return false;
#ifdef RWLOCK
    }
#endif  // RWLOCK

    this->max_rset_ = max(this->max_rset_, (*itr).rcdptr_->tidword_);
  }

  // validate the node set
  for (auto it : node_map_) {
    auto node = (MasstreeWrapper<Tuple>::node_type *) it.first;
    if (node->full_version_value() != it.second) {
      this->status_ = TransactionStatus::aborted;
      return false;
    }
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
  // remove inserted records
  for (auto& we : write_set_) {
    if (we.op_ == OpType::INSERT) {
      Masstrees[get_storage(we.storage_)].remove_value(we.key_);
      delete we.rcdptr_;
    }
  }

  // unlock CLL_
  unlockCLL();

  construct_RLL();

  gc_records();

  read_set_.clear();
  write_set_.clear();
  node_map_.clear();

#if BACK_OFF
#if ADD_ANALYSIS
  uint64_t start(rdtscp());
#endif

  Backoff::backoff(FLAGS_clocks_per_us);

#if ADD_ANALYSIS
  result_->local_backoff_latency_ += rdtscp() - start;
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
    switch ((*itr).op_) {
      case OpType::UPDATE: {
        maxtid.absent = false;
        memcpy((*itr).rcdptr_->body_.get_val_ptr(),
               (*itr).body_.get_val_ptr(), (*itr).body_.get_val_size());
        break;
      }
      case OpType::INSERT: {
        maxtid.absent = false;
        break;
      }
      case OpType::DELETE: {
        maxtid.absent = true;
        Status stat = Masstrees[get_storage((*itr).storage_)].remove_value((*itr).key_);
        gc_records_.push_back((*itr).rcdptr_);
        break;
      }
      default:
        ERR;
    }
    __atomic_store_n(&((*itr).rcdptr_->tidword_.obj_), maxtid.obj_, __ATOMIC_RELEASE);
  }

  unlockCLL();
  RLL_.clear();
  gc_records();
  read_set_.clear();
  write_set_.clear();
  node_map_.clear();
}

bool TxExecutor::commit() {
  if (validation()) {
    writePhase();
    return true;
  } else {
    return false;
  }
}

bool TxExecutor::isLeader() {
  return this->thid_ == 0;
}

void TxExecutor::leaderWork() {
  moccLeaderWork(this->epoch_timer_start, this->epoch_timer_stop);
#if BACK_OFF
  leaderBackoffWork(backoff_, MoccResult);
#endif
}

void TxExecutor::gc_records() {
  const auto r_epoch = ReclamationEpoch;

  // for records
  while (!gc_records_.empty()) {
    Tuple* rec = gc_records_.front();
    if (rec->tidword_.epoch > r_epoch) break;
    delete rec;
    gc_records_.pop_front();
  }
}

void TxExecutor::reconnoiter_begin() {
  reconnoitering_ = true;
}

void TxExecutor::reconnoiter_end() {
  unlockCLL();
  read_set_.clear();
  node_map_.clear();
  reconnoitering_ = false;
  begin();
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

void TxScanCallback::on_resp_node(const MasstreeWrapper<Tuple>::node_type *n, uint64_t version) {
  auto it = tx_->node_map_.find((void*)n);
  if (it == tx_->node_map_.end()) {
    tx_->node_map_.emplace_hint(it, (void*)n, version);
  } else if ((*it).second != version) {
    tx_->status_ = TransactionStatus::aborted;
  }
}
