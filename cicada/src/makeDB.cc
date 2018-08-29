#include <random>
#include <atomic>
#include "include/tuple.hpp"
#include "include/timeStamp.hpp"
#include "include/debug.hpp"
#include "include/common.hpp"

using namespace std;

extern void mutexInit(pthread_mutex_t& lock);

void makeDB() {
	Tuple *hashTmp;
	Version *verTmp;
	random_device rnd;

	try {
		if (posix_memalign((void**)&HashTable, 64, TUPLE_NUM * sizeof(Tuple)) != 0) ERR;
		for (int i = 0; i < TUPLE_NUM; i++) {
			HashTable[i].latest = new Version();
		}
	} catch (bad_alloc) {
		ERR;
	}

	TimeStamp tstmp;
	tstmp.generateTimeStamp(0);
	for (int i = 1; i <= TUPLE_NUM; i++) {
		hashTmp = &HashTable[i % TUPLE_NUM];
		hashTmp->gClock.store(-1, memory_order_release);
		hashTmp->key = i;
		verTmp = hashTmp->latest.load();
		verTmp->wts.store(tstmp.ts, memory_order_release);;
		verTmp->status.store(VersionStatus::committed, std::memory_order_release);
		verTmp->key = i;
		verTmp->val = rnd() % (TUPLE_NUM * 10);
	}

}

