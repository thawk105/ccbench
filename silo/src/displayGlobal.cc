//DISPLAY_GLOBAL_CC
#include <bitset>
#include <iomanip>
#include <limits>
#include "include/common.hpp"
#include "include/debug.hpp"

using namespace std;

void displayDB() {

	Tuple *tuple;

	for (int i = 0; i < TUPLE_NUM; ++i) {
		tuple = &Table[i % TUPLE_NUM];
		cout << "------------------------------" << endl;	//-は30個
		cout << "key: " << tuple->key << endl;
		cout << "val: " << tuple->val << endl;
		cout << "TIDword: " << tuple->tidword.load() << endl;
		cout << "bit: " << static_cast<bitset<64>>(tuple->tidword.load()) << endl;
		cout << endl;
	}
}

void displayPRO() 
{
   	for (int i = 0; i < PRO_NUM; i++) { 
		cout << "transaction No." << i << endl;
		for (int j = 0; j < MAX_OPE; j++) {
	   		cout << "(ope, key, val) = (";
			switch(Pro[i][j].ope){
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

void displayAbortRate() 
{
	long double sumT(0), sumA(0);
	long double rate[THREAD_NUM] = {};
	for (auto i = 1; i < THREAD_NUM; ++i) {
		sumT += FinishTransactions[i].num;
		sumA += AbortCounts[i].num;
		rate[i] = sumA / (sumT + sumA);
	}

	long double ave_rate(0);
	for (auto i = 0; i < THREAD_NUM; ++i) {
		ave_rate += rate[i];
	}
	ave_rate /= (long double) THREAD_NUM;

	cout << ave_rate << endl;
}

