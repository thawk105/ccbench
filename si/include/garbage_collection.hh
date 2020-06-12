#pragma once

#include <atomic>
#include <queue>

#include "../../include/inline.hh"
#include "../../include/result.hh"
#include "si_op_element.hh"
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
  // deque を使うのは，どこまでサイズが肥大するか不明瞭であるから．
  // vector のリサイズは要素の全コピーが発生するなどして重いから．
#ifdef CCTR_ON
  std::deque<TransactionTable *> gcq_for_TMT_;
  std::deque<TransactionTable *> reuse_TMT_element_from_gc_;
#endif  // CCTR_ON
  std::deque <GCElement<Tuple>> gcq_for_versions_;
  std::deque<Version *> reuse_version_from_gc_;
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
