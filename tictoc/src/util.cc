#include <atomic>
#include <cstdint>
#include <iostream>
#include <sys/time.h>

#include "../../include/inline.hpp"
#include "include/common.hpp"
#include "include/debug.hpp"
#include "include/procedure.hpp"
#include "include/random.hpp"
#include "include/transaction.hpp"
#include "include/tuple.hpp"

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

void makeDB() {
	Tuple *tmp;
	Xoroshiro128Plus rnd;
	rnd.init();

	try {
		if (posix_memalign((void**)&Table, 64, (TUPLE_NUM) * sizeof(Tuple)) != 0) ERR;
	} catch (bad_alloc) {
		ERR;
	}

	for (unsigned int i = 0; i < TUPLE_NUM; ++i) {
		tmp = &Table[i];
		tmp->tsw.obj = 0;
		tmp->pre_tsw.obj = 0;
		tmp->key = i;
		tmp->val = rnd.next() % TUPLE_NUM;
	}

}

void 
makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd) {
	for (unsigned int i = 0; i < MAX_OPE; ++i) {
		if ((rnd.next() % 10) < RRATIO) {
			pro[i].ope = Ope::READ;
		} else {
			pro[i].ope = Ope::WRITE;
		}
		pro[i].key = rnd.next() % TUPLE_NUM;
		pro[i].val = rnd.next() % TUPLE_NUM;
	}
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
