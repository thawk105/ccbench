#include "include/common.hpp"
#include "include/debug.hpp"
#include "include/transaction.hpp"
#include "include/version.hpp"
#include <atomic>
#include <algorithm>
#include <bitset>

using namespace std;

void
Transaction::tbegin()
{
	this->txid = TMT[1].prev_cstamp.load(memory_order_acquire);
	for (unsigned int i = 2; i < THREAD_NUM; ++i) {
		this->txid = max(this->txid, TMT[i].prev_cstamp.load(memory_order_acquire));
	}
	ThtxID[thid].num.store(txid, memory_order_release);
}

unsigned int
Transaction::tread(unsigned int key)
{
	return 0;
}

void
Transaction::twrite(unsigned int key, unsigned int val)
{
}

bool 
Transaction::commit()
{
	FinishTransactions[thid].num++;
	return true;
}

void
Transaction::abort()
{
	AbortCounts[thid].num++;
}

void
Transaction::rwsetClear()
{
	//printf("thread #%d: readSet size -> %d\n", thid, readSet.size());
	//printf("thread #%d: writeSet size -> %d\n", thid, writeSet.size());

	readSet.clear();
	writeSet.clear();

	//printf("thread #%d: readSet size -> %d\n", thid, readSet.size());
	//printf("thread #%d: writeSet size -> %d\n", thid, writeSet.size());

	return;
}
