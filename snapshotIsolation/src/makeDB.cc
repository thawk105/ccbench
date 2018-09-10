#include <random>
#include <stdlib.h>
#include <atomic>
#include "include/tuple.hpp"
#include "include/debug.hpp"
#include "include/common.hpp"

using namespace std;

void
makeDB()
{
	Tuple *tmp;
	Version *verTmp;
	random_device rnd;

	try {
		if (posix_memalign((void**)&Table, 64, (TUPLE_NUM) * sizeof(Tuple)) != 0) ERR;
		for (unsigned int i = 0; i < TUPLE_NUM; i++) {
			if (posix_memalign((void**)&Table[i].latest, 64, sizeof(Version)) != 0) ERR;
		}
	} catch (bad_alloc) {
		ERR;
	}

	for (unsigned int i = 0; i < TUPLE_NUM; i++) {
		tmp = &Table[i];
		tmp->key = i;
		tmp->lock.store(-1, memory_order_release);
		verTmp = tmp->latest.load(std::memory_order_acquire);
		verTmp->rts.store(0, memory_order_release);
		verTmp->wts = 0;
		verTmp->val = rnd() % (TUPLE_NUM * 10);
		verTmp->prev = nullptr;
		verTmp->status.store(VersionStatus::committed, std::memory_order_release);
	}
}

