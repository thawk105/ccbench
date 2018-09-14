#include "include/transaction.hpp"
#include "include/tuple.hpp"
#include "include/common.hpp"
#include "include/debug.hpp"
#include <algorithm>
#include <bitset>
#include <fstream>
#include <sstream>
#include <stdio.h>
#include <string>
#include <sys/time.h>

#define LOCK_BIT 1

extern bool chkSpan(struct timeval &start, struct timeval &stop, long threshold);
extern void displayDB();

using namespace std;

int 
Transaction::tread(unsigned int key)
{
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		if ((*itr).key == key) return (*itr).val;
	}

	for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
		if ((*itr).key == key) return (*itr).val;
	}


	TsWord v1, v2;
	unsigned int return_val;
	do {
		v1.obj = __atomic_load_n(&(Table[key].tsw.obj), __ATOMIC_ACQUIRE);
		return_val = Table[key].val;
		v2.obj = __atomic_load_n(&(Table[key].tsw.obj), __ATOMIC_ACQUIRE);
	} while (v1 != v2 || v1.lock & 1);

	readSet.push_back(SetElement(key, return_val, v1));

	return 0;
}

void 
Transaction::twrite(unsigned int key, unsigned int val)
{
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		if ((*itr).key == key) {
			(*itr).val = val;
			return;
		}
	}

	TsWord tsword;
	tsword.obj = __atomic_load_n(&(Table[key].tsw.obj), __ATOMIC_ACQUIRE);
	writeSet.push_back(SetElement(key, val, tsword));
}

bool 
Transaction::validationPhase()
{
	lockWriteSet();

	// step2, compute the commit timestamp
	commit_ts = 0;
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		uint64_t e_tuple_rts;
		TsWord loadtsw;
	    loadtsw.obj	= __atomic_load_n(&(Table[(*itr).key].tsw.obj), __ATOMIC_ACQUIRE);
		e_tuple_rts = loadtsw.wts + loadtsw.delta + 1;
		commit_ts = max(commit_ts, e_tuple_rts);
	}	

	for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
		commit_ts = max(commit_ts, (*itr).tsw.wts);
	}


	// step3, validate the read set.
	for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {

		TsWord v1, v2;
		bool success;
		do {
			success = true;
		    v1.obj 	= __atomic_load_n(&(Table[(*itr).key].tsw.obj), __ATOMIC_ACQUIRE);
			
			asm volatile("" ::: "memory");

			if ((*itr).tsw.wts != v1.wts) {
				///cout << "itr.tsw.wts: " << (*itr).tsw.wts << endl;
				///cout << "v1.wts: " << v1.wts << endl;
				return false;
			}

			if ((v1.wts + v1.delta) < commit_ts && v1.lock) {
				bool includeW = false;
				for (auto itr_w = writeSet.begin(); itr_w != writeSet.end(); ++itr_w) {
					if ((*itr).key == (*itr_w).key) {
						includeW = true;
						break;
					}
				}
				if (!includeW) {
					return false;
				}
			}

			//extend the rts of the tuple
			if ((v1.wts + v1.delta) < commit_ts) {
				// Handle delta overflow
				uint64_t delta = commit_ts - v1.wts;
				uint64_t shift = delta - (delta & 0x7fff);
				v2.obj = v1.obj;
				v2.wts = v2.wts + shift;
				v2.delta = delta - shift;
				success = __atomic_compare_exchange_n(&(Table[(*itr).key].tsw.obj), &(v1.obj), v2.obj, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
			}
		} while (!success);
	}

	return true;
}

void 
Transaction::abort() 
{
	unlockWriteSet();
	AbortCounts[thid].num++;

	readSet.clear();
	writeSet.clear();
}

void 
Transaction::writePhase()
{
	TsWord result;
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		Table[(*itr).key].val = (*itr).val;
		result.wts = this->commit_ts;
		result.delta = 0;
		result.lock = 0;
		__atomic_store_n(&(Table[(*itr).key].tsw.obj), result.obj, __ATOMIC_RELEASE);
	}

	readSet.clear();
	writeSet.clear();
	FinishTransactions[thid].num++;
}

void 
Transaction::lockWriteSet()
{
	TsWord expected, desired;

	sort(writeSet.begin(), writeSet.end());
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		//lock
		expected.obj = __atomic_load_n(&(Table[(*itr).key].tsw.obj), __ATOMIC_ACQUIRE);
		do {
			while (expected.lock) {
				expected.obj = __atomic_load_n(&(Table[(*itr).key].tsw.obj), __ATOMIC_ACQUIRE);
			}
			desired = expected;
			desired.lock = 1;
		} while (!__atomic_compare_exchange_n(&(Table[(*itr).key].tsw.obj), &(expected.obj), desired.obj, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE));
	}
}

void 
Transaction::unlockWriteSet()
{
	TsWord expected, desired;

	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		//unlock
		expected.obj = __atomic_load_n(&(Table[(*itr).key].tsw.obj), __ATOMIC_ACQUIRE);
		desired.obj = expected.obj;
		desired.lock = 0;
		__atomic_store_n(&(Table[(*itr).key].tsw.obj), desired.obj, __ATOMIC_RELEASE);
	}
}

void
Transaction::dispWS()
{
	cout << "th " << this->thid << ": write set: ";

	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		cout << "(" << (*itr).key << ", " << (*itr).val << "), ";
	}
	cout << endl;
}
