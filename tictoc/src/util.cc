#include <atomic>
#include <cstdint>
#include <iostream>
#include <sys/time.h>

#include "../../include/inline.hpp"
#include "include/common.hpp"
#include "include/transaction.hpp"

using namespace std;

bool
chkSpan(struct timeval &start, struct timeval &stop, long threshold)
{
	long diff = 0;
	diff += (stop.tv_sec - start.tv_sec) * 1000 * 1000 + (stop.tv_usec - start.tv_usec);
	if (diff > threshold) return true;
	else return false;
}

bool
chkClkSpan(uint64_t &start, uint64_t &stop, uint64_t threshold)
{
	uint64_t diff = 0;
	diff = stop - start;
	if (diff > threshold) return true;
	else return false;
}

void 
prtRslt(uint64_t &bgn, uint64_t &end)
{
	uint64_t diff = end - bgn;
	uint64_t sec = diff / CLOCK_PER_US / 1000 / 1000;

	uint64_t result = (double)FinishTransactions / (double)sec;
	cout << (int)result << endl;
}

void
sumTrans(Transaction *trans) 
{
	uint64_t expected, desired;

	expected = FinishTransactions.load(memory_order_acquire);
	for (;;) {
		desired = expected + trans->finishTransactions;
		if (FinishTransactions.compare_exchange_weak(expected, desired, memory_order_acq_rel, memory_order_acquire)) break;
	}

	expected = AbortCounts.load(memory_order_acquire);
	for (;;) {
		desired = expected + trans->abortCounts;
		if (AbortCounts.compare_exchange_weak(expected, desired, memory_order_acq_rel, memory_order_acquire)) break;
	}

	expected = Rtsudctr.load(memory_order_acquire);
	for (;;) {
		desired = expected + trans->rtsudctr;
		if (Rtsudctr.compare_exchange_weak(expected, desired, memory_order_acq_rel, memory_order_acquire)) break;
	}

	expected = Rts_non_udctr.load(memory_order_acquire);
	for (;;) {
		desired = expected + trans->rts_non_udctr;
		if (Rts_non_udctr.compare_exchange_weak(expected, desired, memory_order_acq_rel, memory_order_acquire)) break;
	}
}
