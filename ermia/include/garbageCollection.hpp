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
  uint32_t fmin, fmax; // first range of txid in TMT.
  uint32_t smin, smax; // second range of txid in TMT.

  static std::atomic<uint32_t> gcThreshold; // share for all object (meaning all thread).

public:
  std::queue<TransactionTable *> gcqForTMT;
  std::queue<GCElement<Tuple>> gcqForVersion;
  uint8_t thid_;

  GarbageCollection() {}
  GarbageCollection(uint8_t thid) : thid_(thid) {}
  void set_thid_(uint8_t thid) { thid_ = thid; }

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
  void gcVersion(ErmiaResult &rsob);
  void gcTMTelement(ErmiaResult &rsob);
  // -----
};

#ifdef GLOBAL_VALUE_DEFINE
  // declare in ermia.cc
  std::atomic<uint32_t> GarbageCollection::gcThreshold(0);
#endif
