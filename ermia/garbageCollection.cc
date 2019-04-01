
#include <algorithm>
#include <iostream>

#include "../include/debug.hpp"

#include "include/common.hpp"
#include "include/garbageCollection.hpp"
#include "include/result.hpp"
#include "include/transaction.hpp"
#include "include/version.hpp"

using std::cout, std::endl;

// start, for leader thread.
bool
GarbageCollection::chkSecondRange()
{
  TransactionTable *tmt;

  smin = UINT32_MAX;
  smax = 0;
  for (unsigned int i = 1; i < THREAD_NUM; ++i) {
    tmt = __atomic_load_n(&TMT[i], __ATOMIC_ACQUIRE);
    uint32_t tmptxid = tmt->txid.load(std::memory_order_acquire);
    smin = min(smin, tmptxid);
    smax = max(smax, tmptxid);
  }

  //cout << "fmin, fmax : " << fmin << ", " << fmax << endl;
  //cout << "smin, smax : " << smin << ", " << smax << endl;

  if (fmax < smin)
    return true;
  else 
    return false;
}

void
GarbageCollection::decideFirstRange()
{
  TransactionTable *tmt;

  fmin = fmax = 0;
  for (unsigned int i = 1; i < THREAD_NUM; ++i) {
    tmt = __atomic_load_n(&TMT[i], __ATOMIC_ACQUIRE);
    uint32_t tmptxid = tmt->txid.load(std::memory_order_acquire);
    fmin = min(fmin, tmptxid);
    fmax = max(fmax, tmptxid);
  }

  return;
}
// end, for leader thread.

// for worker thread
void
GarbageCollection::gcVersion(ErmiaResult &rsob)
{
  uint32_t threshold = getGcThreshold();

  // my customized Rapid garbage collection inspired from Cicada (sigmod 2017).
  while (!gcqForVersion.empty()) {
    if ((gcqForVersion.front().cstamp >> 1) >= threshold) break;

    // (a) acquiring the garbage collection lock succeeds
    uint8_t zero = 0;
    if (!Table[gcqForVersion.front().key].gClock.compare_exchange_strong(zero, this->thid, std::memory_order_acq_rel, std::memory_order_acquire)) {
      // fail acquiring the lock
      gcqForVersion.pop();
      continue;
    }

    // (b) v.cstamp > record.min_cstamp
    // If not satisfy this condition, (cstamp <= min_cstamp)
    // the version was cleaned by other threads
    if (gcqForVersion.front().cstamp <= Table[gcqForVersion.front().key].min_cstamp) {
      // releases the lock
      Table[gcqForVersion.front().key].gClock.store(0, std::memory_order_release);
      gcqForVersion.pop();
      continue;
    }
    // this pointer may be dangling.
       
    Version *delTarget = gcqForVersion.front().ver->committed_prev;
    if (delTarget == nullptr) {
      Table[gcqForVersion.front().key].gClock.store(0, std::memory_order_release);
      gcqForVersion.pop();
      continue;
    }
    delTarget = delTarget->prev;
    if (delTarget == nullptr) {
      Table[gcqForVersion.front().key].gClock.store(0, std::memory_order_release);
      gcqForVersion.pop();
      continue;
    }
 
    // the thread detaches the rest of the version list from v
    gcqForVersion.front().ver->committed_prev->prev = nullptr;
    // updates record.min_wts
    Table[gcqForVersion.front().key].min_cstamp.store(gcqForVersion.front().ver->committed_prev->cstamp, memory_order_release);

    while (delTarget != nullptr) {
      //next pointer escape
      Version *tmp = delTarget->prev;
      delete delTarget;
      delTarget = tmp;
      ++rsob.localGCVersionCounts;
    }

    // releases the lock
    Table[gcqForVersion.front().key].gClock.store(0, std::memory_order_release);
    gcqForVersion.pop();
  }

  return;
}

void
GarbageCollection::gcTMTelement(ErmiaResult &rsob)
{
  uint32_t threshold = getGcThreshold();
  if (gcqForTMT.empty()) return;

  for (;;) {
    TransactionTable *tmt = gcqForTMT.front();
    if (tmt->txid < threshold) {
      gcqForTMT.pop();
      delete tmt;
      ++rsob.localGCTMTElementsCounts;
      if (gcqForTMT.empty()) break;
    }
    else break;
  }
  
  return;
}

