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
#include "garbage_collection.hh"
#include "tuple.hh"
#include "version.hh"

enum class TransactionStatus : uint8_t {
  inFlight,
  committing,
  committed,
  aborted,
};

class TxExecutor {
public:
  uint8_t thid_;         // thread ID
  uint32_t cstamp_ = 0;  // Transaction end time, c(T)
  uint32_t
          txid_;  // TID and begin timestamp - the current log sequence number (LSN)
  uint32_t pre_gc_threshold_ = 0;
  uint64_t gcstart_, gcstop_;
  char return_val_[VAL_SIZE] = {};
  char write_val_[VAL_SIZE] = {};

  std::vector<SetElement<Tuple>> read_set_;
  std::vector<SetElement<Tuple>> write_set_;
  std::vector<Procedure> pro_set_;

  GarbageCollection gcobject_;
  Result *sres_;
  TransactionStatus status_ =
          TransactionStatus::inFlight;  // Status: inFlight, committed, or aborted

  TxExecutor(uint8_t thid, unsigned int max_ope, Result *sres)
          : thid_(thid), sres_(sres) {
    gcobject_.thid_ = thid;
    read_set_.reserve(max_ope);
    write_set_.reserve(max_ope);
    pro_set_.reserve(max_ope);

    if (FLAGS_pre_reserve_tmt_element) {
      for (size_t i = 0; i < FLAGS_pre_reserve_tmt_element; ++i)
        gcobject_.reuse_TMT_element_from_gc_.emplace_back(
                new TransactionTable());
    }

    if (FLAGS_pre_reserve_version) {
      for (size_t i = 0; i < FLAGS_pre_reserve_version; ++i) {
        gcobject_.reuse_version_from_gc_.emplace_back(new Version());
      }
    }

    genStringRepeatedNumber(write_val_, VAL_SIZE, thid);
  }

  SetElement<Tuple> *searchReadSet(uint64_t key);

  SetElement<Tuple> *searchWriteSet(uint64_t key);

  void tbegin();

  void tread(uint64_t key);

  void twrite(uint64_t key);

  void commit();

  void abort();

  void mainte();

  void dispWS();

  void dispRS();

  static Tuple *get_tuple(Tuple *table, uint64_t key) { return &table[key]; }
};
