#pragma once

#include <atomic>
#include <queue>
#include "version.hpp"

// forward declaration
class TransactionTable;

class GCElement {
public:
  unsigned int key;
  Version *ver;
  uint32_t cstamp;

  GCElement(unsigned int key, Version *ver, uint32_t cstamp) {
    this->key = key;
    this->ver = ver;
    this->cstamp = cstamp;
  }
};

class GarbageCollection {
private:
	uint32_t fmin, fmax; // first range of txid in TMT.
	uint32_t smin, smax; // second range of txid in TMT.

	static std::atomic<uint32_t> gcThreshold; // share for all object (meaning all thread).

public:
	std::queue<TransactionTable *> gcqForTMT;
  std::queue<GCElement> gcqForVersion;
  uint8_t thid;

	// for all thread
	uint32_t getGcThreshold() {
		return gcThreshold.load(std::memory_order_acquire);
	}
	// -----
	
	// for leader thread
	bool chkSecondRange();
	void decideFirstRange();

	void decideGcThreshold() {
		gcThreshold.store(fmin, std::memory_order_release);
	}

	void mvSecondRangeToFirstRange() {
		fmin = smin;
		fmax = smax;
	}
	// -----
	
	// for worker thread
	void gcVersion();
	void gcTMTelement();
	// -----
};

#ifdef GLOBAL_VALUE_DEFINE
	// declare in ermia.cc
	std::atomic<uint32_t> GarbageCollection::gcThreshold(0);
#endif
