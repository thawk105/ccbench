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
		cout << "TS_word: " << tuple->tsw.obj << endl;
		cout << "bit: " << static_cast<bitset<64>>(tuple->tsw.obj) << endl;
		cout << endl;
	}
}

void 
displayPRO(Procedure *pro) 
{
	for (unsigned int i = 0; i < MAX_OPE; i++) {
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
	cout << "FinishTransactions: ";
	cout << FinishTransactions << endl;
}

void
displayAbortCounts()
{
	cout << "AbortCounts: ";
	cout << AbortCounts << endl;
}

void
displayRtsudRate()
{
	// read timestamp update rate
	long double sum(Rtsudctr + Rts_non_udctr), ud(Rtsudctr);
	cout << "Read timestamp update rate: ";
	cout << ud / sum << endl;
}

void 
displayAbortRate() 
{
	long double sum(FinishTransactions + AbortCounts), ab(AbortCounts);
	cout << ab / sum << endl;
}

