#include "include/common.hpp"
#include "include/garbageCollection.hpp"
#include "include/transaction.hpp"
#include <algorithm>
#include <iostream>

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
GarbageCollection::gcTuple()
{
	return;
}

void
GarbageCollection::gcTMTelement(uint32_t threshold)
{
	uint gcctr(0);
	if (gcqForTMT.empty()) return;

	for (;;) {
		TransactionTable *tmt = gcqForTMT.front();
		if (tmt->txid < threshold) {
			gcqForTMT.pop();
			delete tmt;
			++gcctr;
			if (gcqForTMT.empty()) break;
		}
		else break;
	}
	
	return;
}

