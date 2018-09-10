//DISPLAY_GLOBAL_CC
#include <bitset>
#include <iomanip>
#include <limits>
#include "include/common.hpp"
#include "include/debug.hpp"

using namespace std;

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

			version = version->prev;
		}
		cout << endl;
	}
}

void
displayPRO()
{
	for (unsigned int i = 0; i < PRO_NUM; ++i) {
		cout << "transaction No." << i << endl;
		for (unsigned int j = 0; j < MAX_OPE; ++j) {
			cout << "(ope, key, val) = (";
			switch (Pro[i][j].ope) {
				case Ope::READ:
					cout << "READ";
					break;
				case Ope::WRITE:
					cout << "WRITE";
					break;
				default:
					break;
			}
			cout << ", " << Pro[i][j].key
				<< ", " << Pro[i][j].val << ")" << endl;
		}
	}
}

void
displayAbortRate()
{
	long double sumT(0), sumA(0);
	long double rate[THREAD_NUM] = {};

	for (unsigned int i = 1; i < THREAD_NUM; ++i) {
		sumT += FinishTransactions[i].num;
		sumA += AbortCounts[i].num;
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
		sum += AbortCounts[i].num;
	}

	cout << "Abort counts : " << sum << endl;
}
