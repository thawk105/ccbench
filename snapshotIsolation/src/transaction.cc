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
	this->status = TransactionStatus::inFlight;

	this->txid = TMT[1].prev_cstamp.load(memory_order_acquire);
	for (unsigned int i = 2; i < THREAD_NUM; ++i) {
		this->txid = max(this->txid, TMT[i].prev_cstamp.load(memory_order_acquire));
	}
	ThtxID[thid].num.store(txid, memory_order_release);
}

unsigned int
Transaction::tread(unsigned int key)
{
	//if it already access the key object once.
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		if ((*itr).key == key) {
			return (*itr).ver;
		}
	}

	for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
		if ((*itr).key == key) {
			return (*itr).ver->val;
		}
	}

	Version *ver = Table[key].latest.load(memory_order_acquire);
	
	if (ver->status.load(memory_order_acquire) != VersionStatus::committed) {
		ver = ver->committed_prev;
	}

	uint64_t loadrts = ver->rts.load(memory_order_acquire);
	this->readSet.push_back(ReadElement(ver, loadrts));
	
	return ver->val;
}

void
Transaction::twrite(unsigned int key, unsigned int val)
{
	//include writeSet
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		if ((*itr).key == key) {
			(*itr).val = val;
			return;
		}
	}

	Version *source = able[key].latest.load(memory_order_acquire);

	while (source->status.load(memory_order_acquire) != VersionStatus::committed) {
		ver = ver->prev;
	}

	if (!lock(key)) {
		this->status = TransactionStatus::aborted;
		return;
	} else {
		Version *version = new Version(val);
		this->writeSet.push_back(WriteElement(key, ));
	}
}

bool
Transaction::lock(unsigned int key)
{
	signed char expected, desired;
	do {
		expected = Table[key].lock.load(memory_order_acquire);
		if (expected != -1) return false;
	} while (Table[key].lock.compare_exchange_weak(expected, this->thid, memory_order_acq_rel));

	return true;
}
bool 
Transaction::commit()
{
	for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {


	// unlock writeSet
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		Table[(*itr).key].lock.store(-1, memory_order_release);
	}

	FinishTransactions[thid].num++;
	return true;
}

void
Transaction::abort()
{
	AbortCounts[thid].num++;

	// unlock writeSet
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		Table[(*itr).key].lock.store(-1, memory_order_release);
	}
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
