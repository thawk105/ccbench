#include <atomic>
#include "include/debug.hpp"
#include "include/common.hpp"
#include "include/random.hpp"
#include "include/timeStamp.hpp"
#include "include/tuple.hpp"

using namespace std;

void makeDB() {
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
	InitialWts = tstmp.ts;
	for (unsigned int i = 0; i < TUPLE_NUM; i++) {
		tmp = &Table[i];
		tmp->min_wts = tstmp.ts;
		tmp->key = i;
		tmp->gClock.store(0, memory_order_release);
		verTmp = tmp->latest.load();
		verTmp->wts.store(tstmp.ts, memory_order_release);;
		verTmp->status.store(VersionStatus::committed, std::memory_order_release);
		verTmp->key = i;
		verTmp->val = rnd.next() % (TUPLE_NUM * 10);
	}

}

