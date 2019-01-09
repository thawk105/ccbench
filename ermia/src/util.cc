#include <atomic>
#include <bitset>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdlib.h>

#include "include/common.hpp"
#include "include/debug.hpp"
#include "include/procedure.hpp"
#include "include/random.hpp"
#include "include/tuple.hpp"

bool chkClkSpan(uint64_t &start, uint64_t &stop, uint64_t threshold)
{
	uint64_t diff = 0;
	diff = stop - start;
	if (diff > threshold) return true;
	else return false;
}

void
displayDB()
{
	Tuple *tuple;
	Version *version;

	for (unsigned int i = 0; i < TUPLE_NUM; ++i) {
		tuple = &Table[i % TUPLE_NUM];
		cout << "------------------------------" << endl;	// - 30
		cout << "key:	" << tuple->key << endl;

		version = tuple->latest.load(std::memory_order_acquire);
		while (version != NULL) {
			cout << "val:	" << version->val << endl;

			switch (version->status) {
				case VersionStatus::inFlight:
					cout << "status:	inFlight";
					break;
				case VersionStatus::aborted:
					cout << "status:	aborted";
					break;
				case VersionStatus::committed:
					cout << "status:	committed";
					break;
			}
			cout << endl;

			cout << "cstamp:	" << version->cstamp << endl;
			cout << "pstamp:	" << version->psstamp.pstamp << endl;
			cout << "sstamp:	" << version->psstamp.sstamp << endl;
			cout << endl;

			version = version->prev;
		}
		cout << endl;
	}
}

void
displayPRO(Procedure *pro)
{
	for (unsigned int i = 0; i < MAX_OPE; ++i) {
		cout << "(ope, key, val) = (";
		switch (pro[i].ope) {
		case Ope::READ:
				cout << "READ";
				break;
			case Ope::WRITE:
				cout << "WRITE";
			break;
			default:
				break;
		}
		cout << ", " << pro[i].key
			<< ", " << pro[i].val << ")" << endl;
	}
}

void
displayAbortRate()
{
	long double sumT(0), sumA(0);
	long double rate[THREAD_NUM] = {};

	for (unsigned int i = 1; i < THREAD_NUM; ++i) {
		sumT += FinishTransactions[i];
		sumA += AbortCounts[i];
		rate[i] = sumA / (sumT + sumA);
	}

	long double ave_rate(0);
	for (unsigned int i = 0; i < THREAD_NUM; ++i) {
		ave_rate += rate[i];
	}

	ave_rate /= (long double) THREAD_NUM;

	cout << ave_rate << endl;
}

void 
displayAbortCounts()
{
	uint64_t sum = 0;
	for (unsigned int i = 0; i < THREAD_NUM; ++i) {
		sum += AbortCounts[i];
	}

	cout << "Abort counts : " << sum << endl;
}

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
		//verTmp->pstamp = 0;
		//verTmp->sstamp = UINT64_MAX & ~(1);
		verTmp->psstamp.pstamp = 0;
		verTmp->psstamp.sstamp = UINT32_MAX & ~(1);
		// cstamp, sstamp の最下位ビットは TID フラグ
		// 1の時はTID, 0の時はstamp
		verTmp->val = rnd.next() % TUPLE_NUM;
		verTmp->prev = nullptr;
		verTmp->committed_prev = nullptr;
		verTmp->status.store(VersionStatus::committed, std::memory_order_release);
		verTmp->readers.store(0, std::memory_order_release);
	}
}

void
makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd)
{
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


