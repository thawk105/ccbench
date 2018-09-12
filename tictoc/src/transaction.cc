#include "include/transaction.hpp"
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

	uint64_t v1, v2;
	unsigned int return_val;
	do {
		v1 = t->tsword.load(memory_order_acquire);
		return_val = t->val;
		v2 = t->tsword.load(memory_order_acquire);
	} while (v1 != v2 || v1 & 1);

	readSet.push_back(SetElement(key, return_val, v1));

	return 0;
}

void Transaction::twrite(unsigned int key, unsigned int val)
{
	uint64_t tsword = Table[key].tsword.load(memory_order_acquire);
	writeSet.push_back(SetElement(key, val, tsword));
}

bool Transaction::validationPhase()
{
	lockWriteSet();

	// step2, compute the commit timestamp
	uint64_t commit_ts = 0;
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		uint64_t e_tuple_rts;
		uint64_t loadtsw = Table[(*itr).key].tsword.load(memory_order_acquire);
		e_tuple_rts = (loadtsw >> 16) + ((loadtsw >> 1) & 0x7fff) + 1;
		commit_ts = max(commit_ts, e_tuple_rts);
	}	
	for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
		commit_ts = max(commit_ts, ((*itr).tsword >> 16));
	}

	// step3, validate the read set.
	for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {

		bool success = true;
		do {
			uint64_t v1 = Table[(*itr).key].tsword.load(memory_order_acquire);
			uint64_t v2 = v1;
			uint64_t v1_rts = (v1 >> 16) + ((v1 >> 1) & 0x7fff);
			
			if (((*itr).tsword >> 16) != (v1.tsword >> 16)) {
				return false;
			}

			bool not_includeW = true;
			for (auto itr_w = writeSet.begin(); itr_w != writeSet.end(); ++itr) {
				if ((*itr).key == (*itr_w).key) {
					not_includeW = false;
					break;
				}
			}
			if (v1_rts < commit_ts && Table[(*itr).key].isLocked() && not_includeW) {
				return false;
			}

			//extend the rts of the tuple
			if (v1_rts < commit_ts) {
				// Handle delta overflow
				uint64_t delta = commit_ts - (v1 >> 16);
				if (delta > 0x7fff) {
					shift = delta - (delta & 0x7fff);
					v2 = v2 + (shift << 48);
					v2 = v2 - (shift << 1);
				} else {
					v2 = v2

					
				
		} while (!success);
	}

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

	readSet.clear();
	writeSet.clear();
	FinishTransactions[thid].num++;
}

void Transaction::lockWriteSet()
{
	uint64_t expected;

	sort(writeSet.begin(), writeSet.end());
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		//lock
		for (;;) {
			expected = Table[(*itr).key].tsword.load(memory_order_acquire);
			if (!(expected & LOCK_BIT)) {
				if (Table[(*itr).key].tsword.compare_exchange_strong(expected, expected | LOCK_BIT, memory_order_acq_rel)) break;
			}
		}
	}
}

void Transaction::unlockWriteSet()
{
	uint64_t expected;
	uint64_t lockBit = 1;

	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		//unlock
		expected = Table[(*itr).key].tsword.load(memory_order_acquire);
		Table[(*itr).key].tsword.store(expected & (~lockBit), memory_order_release);
	}
}
