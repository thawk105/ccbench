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

int Transaction::tread(unsigned int key)
{
	Tuple *t = &Table[key];

	TsWord v1, v2;
	unsigned int return_val;
	do {
		v1.obj = __atomic_load_n(&(t->tsw.obj), __ATOMIC_ACQUIRE);
		return_val = t->val;
		v2.obj = __atomic_load_n(&(t->tsw.obj), __ATOMIC_ACQUIRE);
	} while (v1 != v2 || v1.lock & 1);

	readSet.push_back(SetElement(key, return_val, v1));

	NNN;
	return 0;
}

void Transaction::twrite(unsigned int key, unsigned int val)
{
	TsWord tsword;
   tsword.obj = __atomic_load_n(&(Table[key].tsw.obj), __ATOMIC_ACQUIRE);
	writeSet.push_back(SetElement(key, val, tsword));
}

bool Transaction::validationPhase()
{
	// if it can execute lock write set with __ATOMIC_RELAXED, i want to do.
	lockWriteSet();

	__atomic_thread_fence(__ATOMIC_ACQ_REL);

	// step2, compute the commit timestamp
	uint64_t commit_ts = 0;
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		uint64_t e_tuple_rts;
		TsWord loadtsw;
	    loadtsw.obj	= __atomic_load_n(&(Table[(*itr).key].tsw.obj), __ATOMIC_ACQUIRE);
		e_tuple_rts = loadtsw.wts + loadtsw.delta + 1;
		commit_ts = max(commit_ts, e_tuple_rts);
	}	

	for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
		TsWord loadtsw;
	    loadtsw.obj	= __atomic_load_n(&(Table[(*itr).key].tsw.obj), __ATOMIC_ACQUIRE);
		commit_ts = max(commit_ts, loadtsw.wts);
	}

	// step3, validate the read set.
	for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {

		bool success = true;
		do {
			TsWord v1, v2;
		    v1.obj 	= __atomic_load_n(&(Table[(*itr).key].tsw.obj), __ATOMIC_ACQUIRE);
			v2 = v1;
			
			if ((*itr).tsw.wts != v1.wts) {
				NNN;
				return false;
			}

			if ((v1.wts + v1.delta) < commit_ts && Table[(*itr).key].tsw.lock) {
				bool not_includeW = true;
				for (auto itr_w = writeSet.begin(); itr_w != writeSet.end(); ++itr) {
					if ((*itr).key == (*itr_w).key) {
						NNN;
						not_includeW = false;
						break;
					}
				}
				if (not_includeW) {
					NNN;
					return false;
				}
			}

			//extend the rts of the tuple
			if ((v1.wts + v1.delta) < commit_ts) {
				// Handle delta overflow
				uint64_t delta = commit_ts - v1.wts;
				uint64_t shift = delta - (delta & 0x7fff);
				v2.wts = v2.wts + shift;
				v2.delta = delta - shift;
				success = __atomic_compare_exchange_n(&(Table[(*itr).key].tsw.obj), &(v1.obj), v2.obj, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
			}
		} while (!success);
	}

	this->commit_ts = commit_ts;
	return true;
}

void Transaction::abort() 
{
	unlockWriteSet();
	AbortCounts[thid].num++;

	readSet.clear();
	writeSet.clear();
}

void Transaction::writePhase()
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

void Transaction::lockWriteSet()
{
	TsWord expected, desired;

	sort(writeSet.begin(), writeSet.end());
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		//lock
		for (;;) {
			expected.obj = __atomic_load_n(&(Table[(*itr).key].tsw.obj), __ATOMIC_ACQUIRE);
			if (!(expected.lock & LOCK_BIT)) {
				desired = expected;
				desired.lock = LOCK_BIT;
				if (__atomic_compare_exchange_n(&(Table[(*itr).key].tsw.obj), &(expected.obj), desired.obj, false, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) break;
			}
		}
	}
}

void Transaction::unlockWriteSet()
{
	TsWord expected, desired;

	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		//unlock
		expected.obj = __atomic_load_n(&(Table[(*itr).key].tsw.obj), __ATOMIC_ACQUIRE);
		desired = expected;
		desired.lock = 0;
		__atomic_store_n(&(Table[(*itr).key].tsw.obj), desired.obj, __ATOMIC_RELEASE);
	}
}
