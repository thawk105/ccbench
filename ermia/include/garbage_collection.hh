#pragma once

#include <atomic>
#include <queue>

#include "../../include/inline.hh"
#include "../../include/op_element.hh"

#include "ermia_op_element.hh"
#include "tuple.hh"
#include "version.hh"

// forward declaration
class TransactionTable;

class GarbageCollection {
private:
  uint32_t fmin_, fmax_;  // first range of txid in TMT.
  uint32_t smin_, smax_;  // second range of txid in TMT.
  static std::atomic <uint32_t>
          GC_threshold_;  // share for all object (meaning all thread).

public:
  std::deque<TransactionTable *> gcq_for_TMT_;
  std::deque<TransactionTable *> reuse_TMT_element_from_gc_;
  std::deque <GCElement<Tuple>> gcq_for_version_;
  std::deque<Version *> reuse_version_from_gc_;
  uint8_t thid_;

  GarbageCollection() {}

  GarbageCollection(uint8_t thid) : thid_(thid) {}

  void set_thid_(uint8_t thid) { thid_ = thid; }

  // for all thread
  INLINE uint32_t getGcThreshold() {
    return GC_threshold_.load(std::memory_order_acquire);
  }
  // -----

  // for leader thread
  bool chkSecondRange();

  void decideFirstRange();

  INLINE void decideGcThreshold() {
    GC_threshold_.store(fmin_, std::memory_order_release);
  }

  INLINE void mvSecondRangeToFirstRange() {
    fmin_ = smin_;
    fmax_ = smax_;
  }
  // -----

  // for worker thread
  void gcVersion(Result *eres_);

  void gcTMTelement(Result *eres_);
  // -----
};

#ifdef GLOBAL_VALUE_DEFINE
// declare in ermia.cc
std::atomic<uint32_t> GarbageCollection::GC_threshold_(0);
#endif
