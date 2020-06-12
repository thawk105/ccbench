#pragma once

#include <cstdint>
#include <map>
#include <vector>

#include "../../include/config.hh"
#include "../../include/procedure.hh"
#include "../../include/result.hh"
#include "../../include/string.hh"
#include "../../include/util.hh"
#include "common.hh"
#include "ermia_op_element.hh"
#include "garbage_collection.hh"
#include "transaction_status.hh"
#include "transaction_table.hh"
#include "tuple.hh"
#include "version.hh"

using namespace std;

class TxExecutor {
public:
  char return_val_[VAL_SIZE] = {};
  char write_val_[VAL_SIZE] = {};
  uint8_t thid_;                  // thread ID
  uint32_t cstamp_ = 0;           // Transaction end time, c(T)
  uint32_t pstamp_ = 0;           // Predecessor high-water mark, η (T)
  uint32_t sstamp_ = UINT32_MAX;  // Successor low-water mark, pi (T)
  uint32_t pre_gc_threshold_ = 0;
  uint32_t
          txid_;  // TID and begin timestamp - the current log sequence number (LSN)
  uint64_t gcstart_, gcstop_;  // counter for garbage collection

  vector <SetElement<Tuple>> read_set_;
  vector <SetElement<Tuple>> write_set_;
  vector <Procedure> pro_set_;

  Result *eres_;
  TransactionStatus status_ =
          TransactionStatus::inFlight;  // Status: inFlight, committed, or aborted
  GarbageCollection gcobject_;

  TxExecutor(uint8_t thid, Result *eres) : thid_(thid), eres_(eres) {
    gcobject_.set_thid_(thid);
    read_set_.reserve(FLAGS_max_ope);
    write_set_.reserve(FLAGS_max_ope);
    pro_set_.reserve(FLAGS_max_ope);

    if (FLAGS_pre_reserve_tmt_element) {
      for (size_t i = 0; i < FLAGS_pre_reserve_tmt_element; ++i)
        gcobject_.reuse_TMT_element_from_gc_.emplace_back(
                new TransactionTable());
    }

    if (FLAGS_pre_reserve_version) {
      for (size_t i = 0; i < FLAGS_pre_reserve_version; ++i)
        gcobject_.reuse_version_from_gc_.emplace_back(new Version());
    }

    genStringRepeatedNumber(write_val_, VAL_SIZE, thid);
  }

  SetElement<Tuple> *searchReadSet(unsigned int key);

  SetElement<Tuple> *searchWriteSet(unsigned int key);

  void tbegin();

  void ssn_tread(uint64_t key);

  void ssn_twrite(uint64_t key);

  void ssn_commit();

  void ssn_parallel_commit();

  void abort();

  void mainte();

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
};
