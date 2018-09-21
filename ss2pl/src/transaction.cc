#include "include/common.hpp"
#include "include/debug.hpp"
#include "include/transaction.hpp"
#include "include/tsc.hpp"

#include <atomic>
#include <stdio.h>

using namespace std;

void
Transaction::abort()
{
	unlock_list();
	AbortCounts[thid].num++;
	if (AbortCounts[thid].num == UINT64_MAX) {
		AbortCounts[thid].num = 0;
		AbortCounts2[thid].num++;
	}

	readSet.clear();
	writeSet.clear();
}

void
Transaction::commit()
{
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		Table[(*itr).key].val = (*itr).val;
	}

	unlock_list();
	FinishTransactions[thid].num++;

	readSet.clear();
	writeSet.clear();
}

void
Transaction::tbegin()
{
	this->status = TransactionStatus::inFlight;
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

	if (Table[key].lock.r_lock()) {
		r_lockList.push_back(&Table[key].lock);
		unsigned int val = Table[key].val;
		readSet.push_back(SetElement(key, val));
		return val;
	} else {
		this->status = TransactionStatus::aborted;
		return -1;
	}
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

	for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
		if ((*itr).key == key) {
			if (!Table[key % TUPLE_NUM].lock.upgrade()) {
				this->status = TransactionStatus::aborted;
				return;
			}
			// upgrade success
			for (auto itr = r_lockList.begin(); itr != r_lockList.end(); ++itr) {
				if (*itr == &(Table[key].lock)) {
					r_lockList.erase(itr);
					w_lockList.push_back(&Table[key].lock);
					writeSet.push_back(SetElement(key, val));
					return;
				}
			}
		}
	}

	// trylock
	if (!Table[key % TUPLE_NUM].lock.w_lock()) {
		this->status = TransactionStatus::aborted;
		return;
	}

	w_lockList.push_back(&Table[key % TUPLE_NUM].lock);
	writeSet.push_back(SetElement(key, val));
	return;
}

void
Transaction::unlock_list()
{
	for (auto itr = r_lockList.begin(); itr != r_lockList.end(); ++itr) {
		(*itr)->r_unlock();
	}
	r_lockList.clear();

	for (auto itr = w_lockList.begin(); itr != w_lockList.end(); ++itr) {
		(*itr)->w_unlock();
	}
	w_lockList.clear();
}

