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

	for (int i = 0; i < TUPLE_NUM; i++) {
		tuple = &Table[i % TUPLE_NUM];
		cout << "------------------------------" << endl;	// - 30
		cout << "key:	" << tuple->key << endl;
		cout << "val:	" << tuple->val << endl;
		cout << endl;
	}
}

void
displayPRO()
{
	for (int i = 0; i < PRO_NUM; i++) {
		cout << "transaction No." << i << endl;
		for (int j = 0; j < MAX_OPE; j++) {
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
