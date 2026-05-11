#pragma once

#include <cstdint>
#include <map>
#include <vector>

#include "../../include/backoff.hh"
#include "../../include/config.hh"
#include "../../include/procedure.hh"
#include "../../include/result.hh"
#include "../../include/status.hh"
#include "../../include/string.hh"
#include "../../include/util.hh"
#include "common.hh"
#include "ermia_op_element.hh"
#include "garbage_collection.hh"
#include "scan_callback.hh"
#include "transaction_status.hh"
#include "transaction_table.hh"
#include "tuple.hh"
#include "version.hh"

using namespace std;

class TxScanCallback;

class TxExecutor {
public:
  uint8_t thid_;                  // thread ID
  uint32_t cstamp_ = 0;           // Transaction end time, c(T)
  uint32_t pstamp_ = 0;           // Predecessor high-water mark, η (T)
  uint32_t sstamp_ = UINT32_MAX;  // Successor low-water mark, pi (T)
  uint32_t pre_gc_threshold_ = 0;
  uint32_t txid_;  // TID and begin timestamp - the current log sequence number (LSN)
  uint64_t gcstart_, gcstop_;  // counter for garbage collection

  vector <SetElement<Tuple>> read_set_;
  vector <SetElement<Tuple>> write_set_;
  std::unordered_map<void*, uint64_t> node_map_;
  vector <Procedure> pro_set_;

  bool reconnoitering_ = false;
  bool is_ronly_ = false;
  bool is_batch_ = false;

  Result *result_;
  TransactionStatus status_ =
          TransactionStatus::inflight;  // Status: inflight, committed, or aborted
  GarbageCollection gcobject_;
  GarbageCollection gcob;
  TxScanCallback callback_;
  Backoff& backoff_;
  const bool& quit_; // for thread termination control

  TxExecutor(uint8_t thid, Backoff& backoff, Result *res, const bool &quit)
    : result_(res), thid_(thid), backoff_(backoff), quit_(quit), callback_(TxScanCallback(this)) {
    gcobject_.set_thid_(thid);

    if (FLAGS_pre_reserve_tmt_element) {
      for (size_t i = 0; i < FLAGS_pre_reserve_tmt_element; ++i)
        gcobject_.reuse_TMT_element_from_gc_.emplace_back(
                new TransactionTable());
    }

    if (FLAGS_pre_reserve_version) {
      for (size_t i = 0; i < FLAGS_pre_reserve_version; ++i)
        gcobject_.reuse_version_from_gc_.emplace_back(new Version());
    }
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

  void ssn_commit();

  void ssn_parallel_commit();

  void abort();

  void mainte();

  bool commit();

  bool isLeader();

  void leaderWork();

  void reconnoiter_begin(); 

  void reconnoiter_end(); 

  Status install_version(Tuple* tuple, Version *ver);

  void verify_exclusion_or_abort();

  void dispWS();

  void dispRS();

  void upReadersBits(Version *ver) {
    uint64_t expected, desired;
    expected = ver->readers_.load(memory_order_acquire);
    for (;;) {
      desired = expected | (1 << thid_);
      if (ver->readers_.compare_exchange_weak(
              expected, desired, memory_order_acq_rel, memory_order_acquire))
        break;
    }
  }

  void downReadersBits(Version *ver) {
    uint64_t expected, desired;
    expected = ver->readers_.load(memory_order_acquire);
    for (;;) {
      desired = expected & ~(1 << thid_);
      if (ver->readers_.compare_exchange_weak(
              expected, desired, memory_order_acq_rel, memory_order_acquire))
        break;
    }
  }

  static INLINE Tuple *get_tuple(Tuple *table, uint64_t key) {
    return &table[key];
  }

  /**
   * @brief Search xxx set
   * @detail Search element of local set corresponding to given key.
   * In this prototype system, the value to be updated for each worker thread 
   * is fixed for high performance, so it is only necessary to check the key match.
   * @param Key [in] the key of key-value
   * @return Corresponding element of local set
   */
  inline SetElement<Tuple> *searchReadSet(Storage s, std::string_view key) {
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
  inline SetElement<Tuple> *searchWriteSet(Storage s, std::string_view key) {
    for (auto &we : write_set_) {
      if (we.storage_ != s) continue;
      if (we.key_ == key) return &we;
    }

    return nullptr;
  }
};
