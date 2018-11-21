#include <atomic>
#include <cstdint>
#include <sys/time.h>
#include "include/debug.hpp"
#include "include/common.hpp"
#include "include/debug.hpp"
#include "include/procedure.hpp"
#include "include/random.hpp"
#include "include/timeStamp.hpp"
#include "include/transaction.hpp"
#include "include/tuple.hpp"

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
makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd) {
	bool ronly = true;

	for (unsigned int i = 0; i < MAX_OPE; ++i) {
		if ((rnd.next() % 10) < RRATIO) {
			pro[i].ope = Ope::READ;
		} else {
			ronly = false;
			pro[i].ope = Ope::WRITE;
		}
		pro[i].key = rnd.next() % TUPLE_NUM;
		pro[i].val = rnd.next() % TUPLE_NUM;
	}
	
	if (ronly) pro[0].ronly = true;
	else pro[0].ronly = false;
}

void
makeDB(uint64_t *initial_wts)
{
	Tuple *tmp;
	Version *verTmp;
	Xoroshiro128Plus rnd;
	rnd.init();

	try {
		if (posix_memalign((void**)&Table, 64, TUPLE_NUM * sizeof(Tuple)) != 0) ERR;
		for (unsigned int i = 0; i < TUPLE_NUM; i++) {
			Table[i].latest = new Version();
		}
	} catch (bad_alloc) {
		ERR;
	}

	TimeStamp tstmp;
	tstmp.generateTimeStampFirst(0);
	*initial_wts = tstmp.ts;
	for (unsigned int i = 0; i < TUPLE_NUM; i++) {
		tmp = &Table[i];
		tmp->min_wts = tstmp.ts;
		tmp->key = i;
		tmp->gClock.store(0, std::memory_order_release);
		verTmp = tmp->latest.load();
		verTmp->wts.store(tstmp.ts, std::memory_order_release);;
		verTmp->status.store(VersionStatus::committed, std::memory_order_release);
		verTmp->key = i;
		verTmp->val = rnd.next() % (TUPLE_NUM * 10);
	}
}

void
prtRslt(uint64_t &bgn, uint64_t &end)
{
	uint64_t diff = end - bgn;
	uint64_t sec = diff / CLOCK_PER_US / 1000 / 1000;

	cout << Finish_transactions / sec << endl;
}

void 
sumTrans(Transaction *trans)
{
	uint64_t expected, desired;

	expected = Finish_transactions.load(std::memory_order_acquire);
	for (;;) {
		desired = expected + trans->finish_transactions;
		if (Finish_transactions.compare_exchange_weak(expected, desired, std::memory_order_acq_rel, std::memory_order_acquire)) break;
	}

	expected = Abort_counts.load(std::memory_order_acquire);
	for (;;) {
		desired = expected + trans->abort_counts;
		if (Abort_counts.compare_exchange_weak(expected, desired, memory_order_acq_rel, memory_order_acquire)) break;
	}
}
