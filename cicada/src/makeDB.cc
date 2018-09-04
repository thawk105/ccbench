#include <random>
#include <atomic>
#include "include/tuple.hpp"
#include "include/timeStamp.hpp"
#include "include/debug.hpp"
#include "include/common.hpp"

using namespace std;

void makeDB() {
	Tuple *tmp;
	Version *verTmp;
	random_device rnd;

	try {
		if (posix_memalign((void**)&Table, 64, TUPLE_NUM * sizeof(Tuple)) != 0) ERR;
		for (unsigned int i = 0; i < TUPLE_NUM; i++) {
			Table[i].latest = new Version();
		}
	} catch (bad_alloc) {
		ERR;
	}

	TimeStamp tstmp;
	tstmp.generateTimeStamp(0);
	for (unsigned int i = 0; i < TUPLE_NUM; i++) {
		tmp = &Table[i];
		tmp->gClock.store(0, memory_order_release);
		tmp->key = i;
		tmp->min_wts = tstmp.ts;
		verTmp = tmp->latest.load();
		verTmp->wts.store(tstmp.ts, memory_order_release);;
		verTmp->status.store(VersionStatus::committed, std::memory_order_release);
		verTmp->key = i;
		verTmp->val = rnd() % (TUPLE_NUM * 10);
	}

}

