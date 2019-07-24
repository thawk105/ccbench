#pragma once

#include <atomic>
#include <queue>

#include "../../include/inline.hpp"
#include "../../include/op_element.hpp"

#include "ermia_op_element.hpp"
#include "result.hpp"
#include "tuple.hpp"
#include "version.hpp"

// forward declaration
class TransactionTable;

class GarbageCollection {
private:
  uint32_t fmin_, fmax_; // first range of txid in TMT.
  uint32_t smin_, smax_; // second range of txid in TMT.
  static std::atomic<uint32_t> gcThreshold_; // share for all object (meaning all thread).

public:
  std::queue<TransactionTable *> gcq_for_TMT_;
  std::queue<GCElement<Tuple>> gcq_for_version_;
  uint8_t thid_;

  GarbageCollection() {}
  GarbageCollection(uint8_t thid) : thid_(thid) {}
  void set_thid_(uint8_t thid) { thid_ = thid; }

  // for all thread
  INLINE uint32_t getGcThreshold() {
    return gcThreshold_.load(std::memory_order_acquire);
  }
  // -----
  
  // for leader thread
  bool chkSecondRange();
  void decideFirstRange();

  INLINE void decideGcThreshold() {
    gcThreshold_.store(fmin_, std::memory_order_release);
  }

  INLINE void mvSecondRangeToFirstRange() {
    fmin_ = smin_;
    fmax_ = smax_;
  }
  // -----
  
  // for worker thread
  void gcVersion(ErmiaResult* eres_);
  void gcTMTelement(ErmiaResult* eres_);
  // -----
};

#ifdef GLOBAL_VALUE_DEFINE
  // declare in ermia.cc
  std::atomic<uint32_t> GarbageCollection::gcThreshold_(0);
#endif
