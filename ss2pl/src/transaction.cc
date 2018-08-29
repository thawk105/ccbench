#include "include/common.hpp"
#include "include/debug.hpp"
#include "include/transaction.hpp"
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
		NNN;
	}

	readSet.clear();
	writeSet.clear();
}

void
Transaction::commit()
{
	for (auto itr = writeSet.begin(); itr != writeSet.end(); itr++) {
		//cout << itr->first << " " << itr->second << endl;
		Table[itr->first % TUPLE_NUM].val = itr->second;
	}

	unlock_list();
	FinishTransactions[thid].num++;

	readSet.clear();
	writeSet.clear();
}

void
Transaction::tbegin(int myid)
{
	this->thid = myid;
	this->status = TransactionStatus::inFlight;
	
	//printf("LockList size %d\n", lockList.size());
	//printf("writeSet size %d\n", writeSet.size());
}

int
Transaction::tread(int key)
{
	auto includeW = writeSet.find(key);
	if (includeW != writeSet.end()) {
		return includeW->second;
	}

	auto includeR = readSet.find(key);
	if (includeR != readSet.end()) {
		return includeR->second;
	}

	if (Table[key % TUPLE_NUM].lock.r_lock()) {
		r_lockList.push_back(&Table[key % TUPLE_NUM].lock);
		int val = Table[key % TUPLE_NUM].val;
		readSet[key] = val;
		return val;
	} else {
		this->status = TransactionStatus::aborted;
		return -1;
	}
}

void
Transaction::twrite(int key, int val)
{
	auto includeW = writeSet.find(key);
	if (includeW != writeSet.end()) {
		includeW->second = val;
		return;
	}

	auto includeR = readSet.find(key);
	if (includeR != readSet.end()) {
		if (!Table[key % TUPLE_NUM].lock.upgrade()) {
			this->status = TransactionStatus::aborted;
			return;
		}
		// upgrade 成功
		for (auto itr = r_lockList.begin(); itr != r_lockList.end(); ++itr) {
			if (*itr == &(Table[key % TUPLE_NUM].lock)) {
				r_lockList.erase(itr);
				break;
			}
		}
	}
	else {
		if (!Table[key % TUPLE_NUM].lock.w_lock()) {
			this->status = TransactionStatus::aborted;
			return;
		}
	}

	w_lockList.push_back(&Table[key % TUPLE_NUM].lock);
	writeSet[key] = val;
}

void
Transaction::unlock_list()
{
	for (auto itr = r_lockList.begin(); itr != r_lockList.end(); ++itr) {
		(*itr)->r_unlock();
	}
	for (auto itr = w_lockList.begin(); itr != w_lockList.end(); ++itr) {
		(*itr)->w_unlock();
	}
}

