#include <algorithm>

#include "include/transaction.hpp"
#include "include/tuple.hpp"

using namespace std;

void
Transaction::begin()
{
	this->status = TransactionStatus::inFlight;

	return;
}

unsigned int
Transaction::read(unsigned int key)
{
	Tuple *tuple = &Table[key % TUPLE_NUM];

	// tuple exists in write set.
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		if ((*itr).key == key) return (*itr).val;
	}

	// tuple exists in read set.
	for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
		if ((*itr).key == key) return tuple->val.load(memory_order_acquire);
	}

	// tuple doesn't exist in read/write set.
	for (auto itr = RLL.begin(); itr != RLL.end(); ++itr) {
		if ((*itr).key == key) {
			lock(tuple, max((*itr).mode, false));
			goto LOCK_FINISH;
		}
	}

	if (tuple->temp.load(memory_order_acquire) >= TEMP_THRESHOLD) {
		lock(tuple, false);
		goto LOCK_FINISH;
	}
LOCK_FINISH:
	if (this->status == TransactionStatus::aborted) return -1;
	
	readSet.push_back(ReadElement(tuple->tidword.load(memory_order_acquire), key));
	return tuple->val.load(memory_order_acquire);
}

void
Transaction::write(unsigned int key, unsigned int val)
{
	// tuple exists in write set.
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		if ((*itr).key == key) {
			(*itr).val = val;
			return;
		}
	}

	Tuple *tuple = &Table[key % TUPLE_NUM];

	for (auto itr = RLL.begin(); itr != RLL.end(); ++itr) {
		if ((*itr).key == key) {
			lock(tuple, max((*itr).mode, true));
			goto LOCK_FINISH;
		}
	}

	if (tuple->temp.load(memory_order_acquire) >= TEMP_THRESHOLD) {
		lock(tuple, true);
		goto LOCK_FINISH;
	}

LOCK_FINISH:
	if (this->status == TransactionStatus::aborted) return;

	//construct log 後で書く
	//-----
	//
	
	writeSet.push_back(WriteElement(key, val));
	return;
}

void
Transaction::lock(Tuple *tuple, bool mode)
{
	vector<LockElement> violations;

	sort(CLL.begin(), CLL.end());
	vector<LockElement>::iterator cllviobgn = CLL.end();
	bool firstvio = true;
	bool upgrade_flag = false;
	// lock exists in CLL (Current Lock List)
	for (auto itr = CLL.begin(); itr != CLL.end(); ++itr) {
		// lock already exists in CLL 
		// 		&& its lock mode is equal to needed mode or it is stronger than needed mode.
		if ((*itr).key == tuple->key) {
			if (mode == (*itr).mode || mode < (*itr).mode) {
				return;
			} else {
				upgrade_flag = true;
			}
		}

		// collect violation
		if ((*itr).key > tuple->key) {
			if (firstvio) {
				cllviobgn = itr;
				firstvio = false;
			}
			violations.push_back(*itr);
		}

	}

	// if too many violations
	if ((CLL.size() / 2) < violations.size() && CLL.size() >= (MAX_OPE / 2)) {
		if (mode) {
			if (tuple->lock.w_trylock()) {
				CLL.push_back(LockElement(tuple->key, &(tuple->lock), true));
				return;
			}
			else {
				this->status = TransactionStatus::aborted;
				return;
			}
		} else {
			if (tuple->lock.r_trylock()) {
				CLL.push_back(LockElement(tuple->key, &(tuple->lock), false));
				return;
			}
			else {
				this->status = TransactionStatus::aborted;
				return;
			}
		}
	}
	else if (!violations.empty()) {
		// Not in canonical mode. Restore.
		for (auto itr = violations.begin(); itr != violations.end(); ++itr) {
			if ((*itr).mode) {
				(*itr).lock->w_unlock();
			} else {
				(*itr).lock->r_unlock();
			}
			RLL.push_back(*itr);
		}
		//delete from CLL
		CLL.erase(cllviobgn, CLL.end());
	}

	sort(RLL.begin(), RLL.end());
	// unconditional lock in canonical mode.
	for (auto itr = RLL.begin(); itr != RLL.end(); ++itr) {
		if ((*itr).key < tuple->key) {
			if ((*itr).mode) {
				(*itr).lock->w_lock();
				CLL.push_back(*itr);
			} else {
				(*itr).lock->r_lock();
				CLL.push_back(*itr);
			}
			rllctr++;
			CLL.push_back(*itr);
		} else {
			break;
		}
	}
	if (rllctr != 0) {
		RLL.erase(RLL.begin(), RLL.begin() + rllctr);
	}

	LockElement acquired_lock(tuple->key, &(tuple->lock), mode);
	if (mode) {
		if (upgrade_flag) tuple->lock.upgrade();
		else tuple->lock.w_lock();	
	}
	else tuple->lock.r_lock();
	CLL.push_back(acquired_lock);

	// remove acquired lock from RLL
	for (auto itr = RLL.begin(); itr != RLL.end(); ++itr) {
		if (acquired_lock == *itr) {
			RLL.erase(itr);
			break;
		}
	}

	return;
}

void
Transaction::construct_rll()
{
	RLL.clear();
	
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		RLL.push_back(LockElement((*itr).key, &(Table[(*itr).key % TUPLE_NUM].lock), true));
	}

	for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
		// check whether itr exists in RLL
		bool includeRLL = false;
		for (auto rll = RLL.begin(); rll != RLL.end(); ++rll) {
			if ((*itr).key == (*rll).key) {
				includeRLL = true;
				break;
			}
		}
		if (includeRLL) continue;
		// r not in RLL
		// if temprature >= threshold
		// 	|| r failed verification
		if (Table[(*itr).key % TUPLE_NUM].temp.load(memory_order_acquire) >= TEMP_THRESHOLD 
				|| (*itr).failed_verification) {
			RLL.push_back(LockElement((*itr).key, &(Table[(*itr).key % TUPLE_NUM].lock), false));
		}
	}

	sort(RLL.begin(), RLL.end());
		
	return;
}

bool
Transaction::commit()
{
	// phase 1 lock write set.
	sort(writeSet.begin(), writeSet.end());
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		lock(&Table[(*itr).key % TUPLE_NUM], true);
	}

	for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
		uint64_t tmptidword = Table[(*itr
	
	return true;
}

void
Transaction::abort()
{
	//unlock CLL
	
	construct_rll();
	AbortCounts[thid].num++;

	readSet.clear();
	writeSet.clear();
	CLL.clear();
	return;
}
