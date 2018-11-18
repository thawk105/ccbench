#include <sys/time.h>
#include <cstdint>

#include "include/common.hpp"
#include "include/transaction.hpp"
#include "include/atomic_tool.hpp"
#include "include/debug.hpp"

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
sumTrans(Transaction *trans)
{
	uint64_t expected, desired;

	expected = FinishTransactions.load(std::memory_order_acquire);
	for (;;) {
		desired = expected + trans->finishTransactions;
		if (FinishTransactions.compare_exchange_weak(expected, desired, std::memory_order_acq_rel, std::memory_order_acquire)) break;
	}

	expected = AbortCounts.load(std::memory_order_acquire);
	for (;;) {
		desired = expected + trans->abortCounts;
		if (AbortCounts.compare_exchange_weak(expected, desired, memory_order_acq_rel, memory_order_acquire)) break;
	}
}

void
prtRslt(uint64_t &bgn, uint64_t &end)
{
	uint64_t diff = end - bgn;
	uint64_t sec = diff / CLOCK_PER_US / 1000 / 1000;

	cout << FinishTransactions / sec << endl;
}

bool
chkEpochLoaded()
{
	uint64_t_64byte nowepo = loadAcquireGE();
//全てのワーカースレッドが最新エポックを読み込んだか確認する．
	for (unsigned int i = 1; i < THREAD_NUM; ++i) {
		if (__atomic_load_n(&(ThLocalEpoch[i].obj), __ATOMIC_ACQUIRE) != nowepo.obj) return false;
	}

	return true;
}

