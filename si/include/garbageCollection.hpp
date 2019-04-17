#pragma once

#include <atomic>
#include <queue>

#include "../../include/inline.hpp"

#include "common.hpp"
#include "result.hpp"
#include "version.hpp"

#include "/home/tanabe/package/tbb/include/tbb/scalable_allocator.h"

// forward declaration
class TransactionTable;

class GCElement {
public:
  unsigned int key;
  Version *ver;
  uint32_t cstamp;

  GCElement() : key(0), ver(nullptr), cstamp(0) {}

  GCElement(unsigned int key, Version *ver, uint32_t cstamp) {
    this->key = key;
    this->ver = ver;
    this->cstamp = cstamp;
  }
};

class GCTMTElement {
public:
  TransactionTable *tmt;

  GCTMTElement() : tmt(nullptr) {}
  GCTMTElement(TransactionTable *tmt_) : tmt(tmt_) {}
};

class GarbageCollection {
private:
  uint32_t fmin, fmax; // first range of txid in TMT.
  uint32_t smin, smax; // second range of txid in TMT.

  static std::atomic<uint32_t> gcThreshold; // share for all object (meaning all thread).

public:
#ifdef CCTR_ON
  std::deque<TransactionTable *, tbb::scalable_allocator<TransactionTable *>> gcqForTMT;
#endif // CCTR_ON
  std::deque<GCElement, tbb::scalable_allocator<GCElement>> gcqForVersion;
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
  void gcVersion(SIResult &rsob);
#ifdef CCTR_ON
  void gcTMTElements(SIResult &rsob);
#endif // CCTR_ON
  // -----
};

#ifdef GLOBAL_VALUE_DEFINE
  // declare in ermia.cc
  std::atomic<uint32_t> GarbageCollection::gcThreshold(0);
#endif
