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

	for (unsigned int i = 0; i < TUPLE_NUM; i++) {
		tuple = &Table[i % TUPLE_NUM];
		cout << "------------------------------" << endl;	// - 30
		cout << "key:	" << tuple->key << endl;
		cout << "val:	" << tuple->val << endl;
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
	for (unsigned int i = 0; i < THREAD_NUM; ++i) {
		sumT = FinishTransactions[i].num;
		sumA = AbortCounts[i].num;
		//cout << AbortCounts2[i].num << endl;
		rate[i] = sumA / (sumT + sumA);
		//cout << "sumT : " << sumT << endl;
		//cout << "sumA : " << sumA << endl;
		//cout << "ratio[" << i << "] : " << ratio[i] << endl;
	}

	double ave_rate(0);
	for (unsigned int i = 0; i < THREAD_NUM; ++i) {
		//cout << ratio[i] << endl;
		ave_rate += rate[i];
	}
	ave_rate /= (long double)THREAD_NUM;

	cout << ave_rate << endl;
}
