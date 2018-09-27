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
	for (unsigned int i = 0; i < TUPLE_NUM; ++i) {
		tuple = &Table[i];
		cout << "------------------------------" << endl;	//-は30個
		cout << "key: " << tuple->key << endl;
		cout << "val: " << tuple->val << endl;
		cout << "TIDword: " << tuple->tidword.obj << endl;
		cout << "bit: " << tuple->tidword.obj << endl;
		cout << endl;
	}
}

void 
displayPRO(Procedure *pro) 
{
	for (unsigned int i = 0; i < MAX_OPE; ++i) {
   		cout << "(ope, key, val) = (";
		switch(pro[i].ope){
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
displayFinishTransactions()
{
	cout << "display FinishTransactions" << endl;
	for (unsigned int i = 0; i < THREAD_NUM; ++i) {
		cout << "th #" << i << ": " << FinishTransactions[i] << endl;
	}
	cout << endl;
}

void
displayAbortCounts()
{
	cout << "display AbortCounts()" << endl;
	for (unsigned int i = 0; i < THREAD_NUM; ++i) {
		cout << "th #" << i << ": " << AbortCounts[i] << endl;
	}
	cout << endl;
}

void
displayTotalAbortCounts()
{
	uint64_t sum(0);
	for (unsigned int i = 0; i < THREAD_NUM; ++i) {
		sum += AbortCounts[i];
	}

	cout << sum << endl;
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

