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

extern bool chkSpan(struct timeval &start, struct timeval &stop, long threshold);
extern void displayDB();

using namespace std;

int Transaction::tread(unsigned int key)
{
}

void Transaction::twrite(unsigned int key, unsigned int val)
{
}

bool Transaction::validationPhase()
{
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
}

void Transaction::lockWriteSet()
{
	uint64_t expected;
	uint64_t lockBit = 0b100;

	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		//lock
		for (;;) {
			expected = Table[(*itr).key].tidword.load(memory_order_acquire);
			if (!(expected & lockBit)) {
				if (Table[(*itr).key].tidword.compare_exchange_strong(expected, expected | lockBit, memory_order_acq_rel)) break;
			}
		}
	}
}

void Transaction::unlockWriteSet()
{
	uint64_t expected;
	uint64_t lockBit = 0b100;

	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		//unlock
		expected = Table[(*itr).key].tidword.load(memory_order_acquire);
		Table[(*itr).key].tidword.store(expected & (~lockBit), memory_order_release);
	}
}
