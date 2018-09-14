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

void
Transaction::tbegin()
{
	this->status = TransactionStatus::inFlight;
	this->commit_ts = 0;
	this->appro_commit_ts = 0;
}

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
		if (v1.lock & 1) {
			if ((v1.wts + v1.delta) < this->appro_commit_ts) {
				// it must check whether this write set include the tuple,
				// but it already checked L31 - L33.
				// so it must abort.
				this->status = TransactionStatus::aborted;
				return 0;
			}
		}
		return_val = Table[key].val;
		v2.obj = __atomic_load_n(&(Table[key].tsw.obj), __ATOMIC_ACQUIRE);
	} while (v1 != v2 || v1.lock & 1);

	this->appro_commit_ts = max(this->appro_commit_ts, v1.wts);
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
	this->appro_commit_ts = max(this->appro_commit_ts, tsword.wts + tsword.delta);
	writeSet.push_back(SetElement(key, val, tsword));
}

bool 
Transaction::validationPhase()
{
	lockWriteSet();
	if (this->status == TransactionStatus::aborted) {
		return false;
	}

	// i must wait lockWriteSet();
	__atomic_thread_fence(__ATOMIC_RELEASE);

	// step2, compute the commit timestamp
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

			if ((*itr).tsw.wts != v1.wts) {
				TsWord pre_v1;
				pre_v1.obj = __atomic_load_n(&(Table[(*itr).key].pre_tsw.obj), __ATOMIC_ACQUIRE);
				if (pre_v1.wts <= commit_ts && commit_ts < v1.wts) {
					goto TIMESTAMP_HISTORY_PASS;
				} else {
					return false;
				}
			}

TIMESTAMP_HISTORY_PASS:

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
	cll.clear();
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
		__atomic_store_n(&(Table[(*itr).key].pre_tsw.obj), (*itr).tsw.obj, __ATOMIC_RELAXED);
		__atomic_store_n(&(Table[(*itr).key].tsw.obj), result.obj, __ATOMIC_RELEASE);
	}

	FinishTransactions[thid].num++;

	readSet.clear();
	writeSet.clear();
	cll.clear();
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
			//no-wait locking in validation
			if (expected.lock) {
				this->status = TransactionStatus::aborted;
				// i must wait to finish *1
				__atomic_thread_fence(__ATOMIC_RELEASE);
				for (auto c_itr = cll.begin(); c_itr != cll.end(); ++c_itr) {
					expected.obj = __atomic_load_n(&(Table[(*itr).key].tsw.obj), __ATOMIC_ACQUIRE);
					desired.obj = expected.obj;
					desired.lock = 0;
					__atomic_store_n(&(Table[(*itr).key].tsw.obj), desired.obj, __ATOMIC_RELEASE);
				}
				return;
			}
			desired = expected;
			desired.lock = 1;
			// *1
		} while (!__atomic_compare_exchange_n(&(Table[(*itr).key].tsw.obj), &(expected.obj), desired.obj, false, __ATOMIC_RELAXED, __ATOMIC_RELAXED));
		cll.push_back((*itr).key);
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
