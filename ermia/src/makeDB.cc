#include <stdlib.h>
#include <atomic>
#include "include/debug.hpp"
#include "include/common.hpp"
#include "include/random.hpp"
#include "include/tuple.hpp"


using namespace std;

void
makeDB()
{
	Tuple *tmp;
	Version *verTmp;
	Xoroshiro128Plus rnd;
	rnd.init();

	try {
		if (posix_memalign((void**)&Table, 64, (TUPLE_NUM) * sizeof(Tuple)) != 0) ERR;
		for (unsigned int i = 0; i < TUPLE_NUM; ++i) {
			if (posix_memalign((void**)&Table[i].latest, 64, sizeof(Version)) != 0) ERR;
		}
	} catch (bad_alloc) {
		ERR;
	}

	for (unsigned int i = 0; i < TUPLE_NUM; ++i) {
		tmp = &Table[i];
		tmp->key = i;
		verTmp = tmp->latest.load(std::memory_order_acquire);
		verTmp->cstamp = 0;
		verTmp->pstamp = 0;
		verTmp->sstamp = UINT64_MAX & ~(1);
		// cstamp, sstamp の最下位ビットは TID フラグ
		// 1の時はTID, 0の時はstamp
		verTmp->val = rnd.next() % TUPLE_NUM;
		verTmp->prev = nullptr;
		verTmp->committed_prev = nullptr;
		verTmp->status.store(VersionStatus::committed, std::memory_order_release);
		verTmp->readers.store(0, memory_order_release);
	}
}

