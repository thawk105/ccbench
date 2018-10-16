#include <algorithm>
#include <cmath>
#include <random>
#include <stdio.h>
#include "include/atomic_tool.hpp"
#include "include/debug.hpp"
#include "include/transaction.hpp"
#include "include/tuple.hpp"

using namespace std;

ReadElement *
Transaction::searchReadSet(unsigned int key)
{
	for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
		if ((*itr).key == key) return &(*itr);
	}

	return nullptr;
}

WriteElement *
Transaction::searchWriteSet(unsigned int key)
{
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		if ((*itr).key == key) return &(*itr);
	}

	return nullptr;
}

LockElement *
Transaction::searchRLL(unsigned int key)
{
	//will do:  binary search
	for (auto itr = RLL.begin(); itr != RLL.end(); ++itr) {
		if ((*itr).key == key) return &(*itr);
	}

	return nullptr;
}

void
Transaction::begin()
{
	this->status = TransactionStatus::inFlight;
	this->max_rset.obj = 0;
	this->max_wset.obj = 0;

	return;
}

unsigned int
Transaction::read(unsigned int key)
{
	unsigned int return_val;
	Tuple *tuple = &Table[key];

	// tuple exists in write set.
	WriteElement *inW = searchWriteSet(key);
	if (inW) return inW->val;

	// tuple exists in read set.
	ReadElement *inR = searchReadSet(key);
	if (inR) return inR->val;

	// tuple doesn't exist in read/write set.
	LockElement *inRLL = searchRLL(key);
	Epotemp loadepot;
	loadepot.obj = __atomic_load_n(&(tuple->epotemp.obj), __ATOMIC_ACQUIRE);

	if (inRLL != nullptr) lock(tuple, inRLL->mode);
	else if (loadepot.temp >= TEMP_THRESHOLD) lock(tuple, false);
	
	if (this->status == TransactionStatus::aborted) return -1;
	
	Tidword expected, desired;
	for (;;) {
		expected.obj = __atomic_load_n(&(tuple->tidword.obj), __ATOMIC_ACQUIRE);

		while (tuple->lock.counter.load(memory_order_acquire) == -1) {
			expected.obj = __atomic_load_n(&(tuple->tidword.obj), __ATOMIC_ACQUIRE);
		}

		return_val = tuple->val;
		desired.obj = __atomic_load_n(&(tuple->tidword.obj), __ATOMIC_ACQUIRE);
		if (expected == desired) break;
	}

	readSet.push_back(ReadElement(expected, key, return_val));
	return return_val;
}

void
Transaction::write(unsigned int key, unsigned int val)
{
	// tuple exists in write set.
	WriteElement *inw = searchWriteSet(key);
	if (inw) {
		inw->val = val;
		return;
	}

	Tuple *tuple = &Table[key];

	LockElement *inRLL = searchRLL(key);
	Epotemp loadepot;
	loadepot.obj = __atomic_load_n(&(tuple->epotemp.obj), __ATOMIC_ACQUIRE);

	if (inRLL || loadepot.temp >= TEMP_THRESHOLD) lock(tuple, true);
	if (this->status == TransactionStatus::aborted) return;
	
	writeSet.push_back(WriteElement(key, val));
	return;
}

void
Transaction::lock(Tuple *tuple, bool mode)
{
	// lock exists in CLL (current lock list)
	sort(CLL.begin(), CLL.end());
	unsigned int vioctr = 0;
	unsigned int threshold;
	bool upgrade = false;
	for (auto itr = CLL.begin(); itr != CLL.end(); ++itr) {
		// lock already exists in CLL 
		// 		&& its lock mode is equal to needed mode or it is stronger than needed mode.
		if ((*itr).key == tuple->key) {
		  if (mode == (*itr).mode || mode < (*itr).mode) return;
			else upgrade = true;
		}

		// collect violation
		if ((*itr).key >= tuple->key) {
			if (vioctr == 0) threshold = (*itr).key;
			
			vioctr++;
		}

	}

	if (vioctr == 0) threshold = -1;

	// if too many violations
	// i set my condition of too many because the original paper of mocc didn't show
	// the condition of too many.
	if ((CLL.size() / 2) < vioctr && CLL.size() >= (MAX_OPE / 2)) {
		if (mode) {
			if (upgrade) {
				if (tuple->lock.upgrade()) {
					for (auto itr = CLL.begin(); itr != CLL.end(); ++itr) {
						if ((*itr).key == tuple->key) {
							(*itr).mode = true;
							break;
						}
					}
					return;
				} else {
					this->status = TransactionStatus::aborted;
					return;
				}
			}

			if (tuple->lock.w_trylock()) {
				CLL.push_back(LockElement(tuple->key, &(tuple->lock), true));
				return;
			} else {
				this->status = TransactionStatus::aborted;
				return;
			}

		} else {
			if (tuple->lock.r_trylock()) {
				CLL.push_back(LockElement(tuple->key, &(tuple->lock), false));
				return;
			} else {
				this->status = TransactionStatus::aborted;
				return;
			}
		}
	}
	
	if (vioctr != 0) {
		// not in canonical mode. restore.
		for (auto itr = CLL.begin() + (CLL.size() - vioctr); itr != CLL.end(); ++itr) {
			if ((*itr).mode) (*itr).lock->w_unlock();
			else (*itr).lock->r_unlock();
		}
		//delete from CLL
		if (CLL.size() == vioctr) CLL.clear();
		else CLL.erase(CLL.begin() + (CLL.size() - vioctr), CLL.end());
	}

	// unconditional lock in canonical mode.
	for (auto itr = RLL.begin(); itr != RLL.end(); ++itr) {
		if ((*itr).key <= threshold) continue;
		if ((*itr).key < tuple->key) {
			if ((*itr).mode) (*itr).lock->w_lock();
			else (*itr).lock->r_lock();
			CLL.push_back(*itr);
		} else break;
	}

	if (mode) tuple->lock.w_lock();	
	else tuple->lock.r_lock();

	CLL.push_back(LockElement(tuple->key, &(tuple->lock), mode));
	return;
}

void
Transaction::construct_RLL()
{
	Tuple *tuple;
	RLL.clear();
	
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		RLL.push_back(LockElement((*itr).key, &(Table[(*itr).key].lock), true));
	}

	for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
		// check whether itr exists in RLL
		if (searchRLL((*itr).key) != nullptr) continue;

		// r not in RLL
		// if temprature >= threshold
		// 	|| r failed verification
		Epotemp loadepot;
		tuple = &Table[(*itr).key];
		loadepot.obj = __atomic_load_n(&(tuple->epotemp.obj), __ATOMIC_ACQUIRE);
		if (loadepot.temp >= TEMP_THRESHOLD 
				|| (*itr).failed_verification) {
			RLL.push_back(LockElement((*itr).key, &(tuple->lock), false));
		}

		// maintain temprature p
		if ((*itr).failed_verification) {
			Epotemp expected, desired;
			uint64_t nowepo;
			expected.obj = __atomic_load_n(&(tuple->epotemp.obj), __ATOMIC_ACQUIRE);
			nowepo = (loadAcquireGE()).obj;

			if (expected.epoch != nowepo) {
				desired.epoch = nowepo;
				desired.temp = 0;
				__atomic_store_n(&(tuple->epotemp.obj), desired.obj, __ATOMIC_RELEASE);
			}

			for (;;) {
				if (expected.temp == TEMP_MAX) break;
				else if (rnd->next() % (1 << expected.temp) == 0) {
					desired = expected;
					desired.temp = expected.temp + 1;
				} else break;
				if (__atomic_compare_exchange_n(&(tuple->epotemp.obj), &(expected.obj), desired.obj, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) break;
			}
		}
	}

	sort(RLL.begin(), RLL.end());
		
	return;
}

bool
Transaction::commit()
{
	Tuple *tuple;
	Tidword expected, desired;

	// phase 1 lock write set.
	sort(writeSet.begin(), writeSet.end());
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		tuple = &Table[(*itr).key];
		lock(tuple, true);
		if (this->status == TransactionStatus::aborted) {
			return false;
		}

		this->max_wset = max(this->max_wset, tuple->tidword);
	}

	asm volatile ("" ::: "memory");
	__atomic_store_n(&(ThLocalEpoch[thid].obj), (loadAcquireGE()).obj, __ATOMIC_RELEASE);
	asm volatile ("" ::: "memory");

	for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
		Tidword check;
		tuple = &Table[(*itr).key];
		check.obj = __atomic_load_n(&(tuple->tidword.obj), __ATOMIC_ACQUIRE);
		if ((*itr).tidword.epoch != check.epoch || (*itr).tidword.tid != check.tid) {
			(*itr).failed_verification = true;
			this->status = TransactionStatus::aborted;
			return false;
		}

		// Silo protocol
		if (tuple->lock.counter.load(memory_order_acquire) == -1) {
			WriteElement *inW = searchWriteSet((*itr).key);
			//if the lock is already acquired and the owner isn't me, abort.
			if (inW == nullptr) {
				this->status = TransactionStatus::aborted;
				return false;
			}
		}
		this->max_rset = max(this->max_rset, tuple->tidword);
	}

	return true;
}

void
Transaction::abort()
{
	//unlock CLL
	unlockCLL();
	
	construct_RLL();

	readSet.clear();
	writeSet.clear();
	return;
}

void
Transaction::unlockCLL()
{
	Tidword expected, desired;

	for (auto itr = CLL.begin(); itr != CLL.end(); ++itr) {
		if ((*itr).mode) (*itr).lock->w_unlock();
		else (*itr).lock->r_unlock();
	}
	CLL.clear();
}

void
Transaction::writePhase()
{
	Tidword tid_a, tid_b, tid_c;

	// calculates (a)
	tid_a = max(this->max_rset, this->max_wset);
	tid_a.tid++;

	//calculates (b)
	//larger than the worker's most recently chosen TID,
	tid_b = mrctid;
	tid_b.tid++;

	// calculates (c)
	tid_c.epoch = ThLocalEpoch[thid].obj;

	// compare a, b, c
	Tidword maxtid = max({tid_a, tid_b, tid_c});
	mrctid = maxtid;

	//write (record, commit-tid)
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		//update nad down lockBit
		Table[(*itr).key].val = (*itr).val;
		__atomic_store_n(&(Table[(*itr).key].tidword.obj), maxtid.obj, __ATOMIC_RELEASE);
	}

	unlockCLL();
	RLL.clear();
	readSet.clear();
	writeSet.clear();
}

void
Transaction::dispCLL()
{
	cout << "th " << this->thid << ": CLL: ";
	for (auto itr = CLL.begin(); itr != CLL.end(); ++itr) {
		cout << (*itr).key << "(";
		if ((*itr).mode) cout << "w) ";
		else cout << "r) ";
	}
	cout << endl;
}

void
Transaction::dispRLL()
{
	cout << "th " << this->thid << ": RLL: ";
	for (auto itr = RLL.begin(); itr != RLL.end(); ++itr) {
		cout << (*itr).key << "(";
		if ((*itr).mode) cout << "w) ";
		else cout << "r) ";
	}
	cout << endl;
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

