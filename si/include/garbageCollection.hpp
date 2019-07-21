#pragma once

#include <atomic>
#include <queue>

#include "../../include/inline.hpp"

#include "common.hpp"
#include "result.hpp"
#include "si_op_element.hpp"
#include "tuple.hpp"
#include "version.hpp"

#include "/home/tanabe/package/tbb/include/tbb/scalable_allocator.h"

// forward declaration
class TransactionTable;

class GarbageCollection {
private:
  uint32_t fmin, fmax; // first range of txid in TMT.
  uint32_t smin, smax; // second range of txid in TMT.

  static std::atomic<uint32_t> gcThreshold; // share for all object (meaning all thread).

public:
#ifdef CCTR_ON
  std::deque<TransactionTable *, tbb::scalable_allocator<TransactionTable *>> gcqForTMT;
#endif // CCTR_ON
  std::deque<GCElement<Tuple>, tbb::scalable_allocator<GCElement<Tuple>>> gcqForVersion;
  uint8_t thid;

  GarbageCollection() {
//#ifdef CCTR_ON
//    gcqForTMT.resize(1000);
//#endif // CCTR_ON
//    gcqForVersion.resize(1000);
  }

  // for all thread
  INLINE uint32_t getGcThreshold() {
    return gcThreshold.load(std::memory_order_acquire);
  }
  // -----
  
  // for leader thread
  bool chkSecondRange();
  void decideFirstRange();

  INLINE void decideGcThreshold() {
    gcThreshold.store(fmin, std::memory_order_release);
  }

  INLINE void mvSecondRangeToFirstRange() {
    fmin = smin;
    fmax = smax;
  }
  // -----
  
  // for worker thread
  void gcVersion(SIResult *sres_);
#ifdef CCTR_ON
  void gcTMTElements(SIResult *sres_);
#endif // CCTR_ON
  // -----
};

#ifdef GLOBAL_VALUE_DEFINE
  // declare in ermia.cc
  std::atomic<uint32_t> GarbageCollection::gcThreshold(0);
#endif
