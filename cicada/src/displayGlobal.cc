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

	for (unsigned int i = 0; i < TUPLE_NUM; i++) {
		tuple = &Table[i % TUPLE_NUM];
		cout << "------------------------------" << endl;	//-は30個
		cout << "key:	" << tuple->key << endl;

		version = tuple->latest;
		while (version != NULL) {
			cout << "val:	" << version->val << endl;
			
			switch (version->status) {
				case VersionStatus::invalid:
					cout << "status:	invalid";
					break;
				case VersionStatus::pending:
					cout << "status:	pending";
					break;
				case VersionStatus::aborted:
					cout << "status:	aborted";
					break;
				case VersionStatus::committed:
					cout << "status:	committed";
					break;
				case VersionStatus::deleted:
					cout << "status:	deleted";
					break;
				default:
					cout << "status error";
					break;
			}
			cout << endl;

			cout << "wts:	" << version->wts << endl;
			cout << "bit:	" << static_cast<bitset<64> >(version->wts) << endl;
			cout << "rts:	" << version->rts << endl;
			cout << "bit:	" << static_cast<bitset<64> >(version->rts) << endl;
			cout << endl;

			version = version->next;
		}
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
displayMinRts() 
{
	cout << "MinRts:	" << MinRts << endl << endl;
}

void 
displayMinWts() 
{
	cout << "MinWts:	" << MinWts << endl << endl;
}

void 
displayThreadWtsArray() 
{
	cout << "ThreadWtsArray:" << endl;
	for (unsigned int i = 0; i < THREAD_NUM; i++) {
		cout << "thid " << i << ": " << ThreadWtsArray[i]->ts << endl;
	}
	cout << endl << endl;
}

void 
displayThreadRtsArray() 
{
	cout << "ThreadRtsArray:" << endl;
	for (unsigned int i = 0; i < THREAD_NUM; i++) {
		cout << "thid " << i << ": " << ThreadRtsArray[i]->ts << endl;
    }
    cout << endl << endl;
}

void
displayFinishTransactions()
{
	cout << "display FinishTransactions" << endl;
	for (unsigned int i = 0; i < THREAD_NUM; ++i) {
		cout << "th #" << i << ": " << FinishTransactions[i].num << endl;
	}
	cout << endl;
}

void
displayAbortCounts()
{
	cout << "display AbortCounts()" << endl;
	for (unsigned int i = 0; i < THREAD_NUM; ++i) {
		cout << "th #" << i << ": " << AbortCounts[i].num << endl;
	}
	cout << endl;
}

void 
displayAbortRate() 
{
	long double sumT(0), sumA(0);
	long double rate[THREAD_NUM] = {};
	for (unsigned int i = 0; i < THREAD_NUM; ++i) {
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
displaySLogSet() 
{
	if (!GROUP_COMMIT) {
	}
	else {
		if (S_WAL) {
			SwalLock.w_lock();
			for (unsigned int i = 0; i < GROUP_COMMIT_INDEX[0].num; ++i) {
				printf("SLogSet[%d]->key, val = (%d, %d)\n", i, SLogSet[i]->key, SLogSet[i]->val);
			}
			SwalLock.w_unlock();

			//if (i == 0) printf("SLogSet is empty\n");
		}
	}
}


