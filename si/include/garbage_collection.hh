#pragma once

#include <atomic>
#include <queue>

#include "../../include/inline.hh"
#include "../../include/result.hh"
#include "common.hh"
#include "si_op_element.hh"
#include "tuple.hh"
#include "version.hh"

#include "/home/tanabe/package/tbb/include/tbb/scalable_allocator.h"

// forward declaration
class TransactionTable;

class GarbageCollection {
 private:
  uint32_t fmin_, fmax_;  // first range of txid in TMT.
  uint32_t smin_, smax_;  // second range of txid in TMT.

  static std::atomic<uint32_t>
      GC_threshold_;  // share for all object (meaning all thread).

 public:
#ifdef CCTR_ON
  std::deque<TransactionTable *, tbb::scalable_allocator<TransactionTable *>>
      gcq_for_TMT_;
#endif  // CCTR_ON
  std::deque<GCElement<Tuple>, tbb::scalable_allocator<GCElement<Tuple>>>
      gcq_for_versions_;
  uint8_t thid_;

  GarbageCollection() {
    //#ifdef CCTR_ON
    //    gcqForTMT.resize(1000);
    //#endif // CCTR_ON
    //    gcqForVersion.resize(1000);
  }

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
  void gcVersion(Result *sres_);
#ifdef CCTR_ON
  void gcTMTElements(Result *sres_);
#endif  // CCTR_ON
  // -----
};

#ifdef GLOBAL_VALUE_DEFINE
// declare in ermia.cc
std::atomic<uint32_t> GarbageCollection::GC_threshold_(0);
#endif
