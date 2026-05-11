#pragma once

#include <stdio.h>
#include <atomic>
#include <iostream>
#include <map>
#include <queue>

#include "../../include/backoff.hh"
#include "../../include/config.hh"
#include "../../include/debug.hh"
#include "../../include/inline.hh"
#include "../../include/procedure.hh"
#include "../../include/result.hh"
#include "../../include/status.hh"
#include "../../include/string.hh"
#include "../../include/util.hh"
#include "../../include/workload.hh"
#include "mvto_op_element.hh"
#include "common.hh"
#include "time_stamp.hh"
#include "tuple.hh"
#include "version.hh"

enum class TransactionStatus : uint8_t {
  invalid,
  inflight,
  committed,
  aborted,
};

class TxExecutor {
public:
  TransactionStatus status_ = TransactionStatus::invalid;
  TimeStamp wts_;
  std::vector<ReadElement<Tuple>> read_set_;
  std::vector<WriteElement<Tuple>> write_set_;
  std::deque<Tuple*> gc_records_;    // for records
  std::deque<GCElement<Tuple>> gcq_; // for versions
  std::vector<Procedure> pro_set_;
  Result *result_ = nullptr;
  const bool& quit_; // for thread termination control

  Backoff& backoff_;

  bool reconnoitering_ = false;
  bool is_ronly_ = false;
  bool is_batch_ = false;

  uint8_t thid_ = 0;
  uint64_t rts_;
  uint64_t start_, stop_;                // for one-sided synchronization
  uint64_t grpcmt_start_, grpcmt_stop_;  // for group commit
  uint64_t gcstart_, gcstop_;            // for garbage collection

  TxExecutor(uint8_t thid, Backoff& backoff, Result *res, const bool &quit)
    : result_(res), thid_(thid), backoff_(backoff), quit_(quit) {

    // wait to initialize MinWts
    while (MinWts.load(memory_order_acquire) == 0);
    rts_ = MinWts.load(memory_order_acquire) - 1;
    wts_.generateTimeStampFirst(thid_);

    __atomic_store_n(&(ThreadWtsArray[thid_].obj_), wts_.ts_, __ATOMIC_RELEASE);
    unsigned int expected, desired;
    expected = FirstAllocateTimestamp.load(memory_order_acquire);
    for (;;) {
      desired = expected + 1;
      if (FirstAllocateTimestamp.compare_exchange_weak(expected, desired,
                                                       memory_order_acq_rel))
        break;
    }

    start_ = rdtscp();
    gcstart_ = start_;
  }

  ~TxExecutor() {
    read_set_.clear();
    write_set_.clear();
    gcq_.clear();
    pro_set_.clear();
  }

  void begin();

  Status read(Storage s, std::string_view key, TupleBody** body);
  Version* read_internal(Storage s, std::string_view key, Tuple* tuple);

  Status write(Storage s, std::string_view key, TupleBody&& body);

  Status insert(Storage s, std::string_view key, TupleBody&& body);

  Status delete_record(Storage s, std::string_view key);

  Status scan(Storage s,
              std::string_view left_key, bool l_exclusive,
              std::string_view right_key, bool r_exclusive,
              std::vector<TupleBody *>&result);

  Status scan(Storage s,
              std::string_view left_key, bool l_exclusive,
              std::string_view right_key, bool r_exclusive,
              std::vector<TupleBody *>&result, int64_t limit);

  bool validation();

  void commit_pending_versions();

  void writePhase();

  bool commit();

  void abort();

  void maintenance();

  bool isLeader();

  void leaderWork();

  void reconnoiter_begin();

  void reconnoiter_end();

  void gc_records();

  void gc_versions();

  void backoff() {
#if ADD_ANALYSIS
    uint64_t start = rdtscp();
#endif
    Backoff::backoff(FLAGS_clocks_per_us);
#if ADD_ANALYSIS
    result_->local_backoff_latency_ += rdtscp() - start;
#endif
  }

  void gcAfterThisVersion([[maybe_unused]] Tuple *tuple, Version *delTarget) {
    while (delTarget != nullptr) {
      // escape next pointer
      Version *tmp = delTarget->next_.load(std::memory_order_acquire);
      delete delTarget;
[[maybe_unused]] gcAfterThisVersion_NEXT_LOOP :
#if ADD_ANALYSIS
      ++result_->local_gc_version_counts_;
#endif
      delTarget = tmp;
    }
  }

  Version *newVersionGeneration([[maybe_unused]] Tuple *tuple, TupleBody&& body) {
#if ADD_ANALYSIS
    ++result_->local_version_malloc_;
#endif
    return new Version(0, this->wts_.ts_, std::move(body));
  }

  void install_version(Tuple* tuple, Version* version) {
    Version* expected;
    for (;;) {
      expected = tuple->ldAcqLatest();
      version->strRelNext(expected);
      if (tuple->latest_.compare_exchange_strong(
          expected, version, memory_order_acq_rel, memory_order_acquire)) {
        break;
      }
    }
  }

  Version* get_latest_previous_version(Version* version) {
    Version* after;
    return get_latest_previous_version(this->wts_.ts_, version, &after);
  }

  Version* get_latest_previous_version(
      uint64_t base_timestamp, Version* version, [[maybe_unused]] Version** after) {
    while (version->ldAcqWts() >= base_timestamp) {
      *after = version;
      version = version->ldAcqNext();
      if (version == nullptr) {
        return nullptr;
      }
    }
    while (version->ldAcqStatus() == VersionStatus::pending);
    while (version->ldAcqStatus() == VersionStatus::aborted) {
      version = version->ldAcqNext();
      if (version == nullptr) {
        return nullptr;
      }
      while (version->ldAcqStatus() == VersionStatus::pending);
    }
    return version;
  }

  void update_rts(Version* version) {
      uint64_t expected = version->ldAcqRts();
      while (true) {
        if (expected > this->wts_.ts_) break;
        if (version->rts_.compare_exchange_strong(
            expected, this->wts_.ts_, memory_order_acq_rel, memory_order_acquire))
          break;
      }
  }

  /**
   * @brief Search xxx set
   * @detail Search element of local set corresponding to given key.
   * In this prototype system, the value to be updated for each worker thread 
   * is fixed for high performance, so it is only necessary to check the key match.
   * @param Key [in] the key of key-value
   * @return Corresponding element of local set
   */
  inline ReadElement<Tuple> *searchReadSet(Storage s, std::string_view key) {
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
  inline WriteElement<Tuple> *searchWriteSet(Storage s, std::string_view key) {
    for (auto &we : write_set_) {
      if (we.storage_ != s) continue;
      if (we.key_ == key) return &we;
    }

    return nullptr;
  }

  void clean_up_read_write_set() {
    for (auto &we: write_set_) {
      if (we.op_ == OpType::INSERT) {
        Masstrees[get_storage(we.storage_)].remove_value(we.key_);
        delete we.rcdptr_;
      } else {
        we.new_ver_->status_.store(VersionStatus::aborted, std::memory_order_release);
      }
    }
    write_set_.clear();
    read_set_.clear();
  }

  static INLINE Tuple *get_tuple(Tuple *table, uint64_t key) {
    return &table[key];
  }
};
