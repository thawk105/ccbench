#include <algorithm>
#include <cmath>
#include <random>
#include <stdio.h>

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

	return;
}

unsigned int
Transaction::read(unsigned int key)
{
	Tuple *tuple = &Table[key];

	// tuple exists in write set.
	WriteElement *inW = searchWriteSet(key);
	if (inW) return inW->val;

	// tuple exists in read set.
	ReadElement *inR = searchReadSet(key);
	if (inR) return inR->val;

	// tuple doesn't exist in read/write set.
	LockElement *inRLL = searchRLL(key);
	if (inRLL) {
		lock(tuple, inRLL->mode);
	} else if (tuple->temp.load(memory_order_acquire) >= TEMP_THRESHOLD) {
		lock(tuple, false);
	}
	
	if (this->status == TransactionStatus::aborted) {
		return -1;
	}
	
	readSet.push_back(ReadElement(tuple->tidword.load(memory_order_acquire), key, tuple->val.load(memory_order_acquire)));
	return tuple->val.load(memory_order_acquire);
}

void
Transaction::write(unsigned int key, unsigned int val)
{
	// tuple exists in write set.
	WriteElement *inW = searchWriteSet(key);
	if (inW) {
		inW->val = val;
		return;
	}

	Tuple *tuple = &Table[key];

	LockElement *inRLL = searchRLL(key);
	if (inRLL || tuple->temp.load(memory_order_acquire) >= TEMP_THRESHOLD) {
		lock(tuple, true);
	}

	if (this->status == TransactionStatus::aborted) {
		return;
	}
	
	writeSet.push_back(WriteElement(key, val));
	return;
}

void
Transaction::lock(Tuple *tuple, bool mode)
{
	// lock exists in CLL (Current Lock List)
	sort(CLL.begin(), CLL.end());
	unsigned int vioctr = 0;
	unsigned int threshold;
	bool upgrade = false;
	for (auto itr = CLL.begin(); itr != CLL.end(); ++itr) {
		// lock already exists in CLL 
		// 		&& its lock mode is equal to needed mode or it is stronger than needed mode.
		if ((*itr).key == tuple->key) {
			if (mode == (*itr).mode || mode < (*itr).mode) {
				return;
			}
			else {
				upgrade = true;
			}
		}

		// collect violation
		if ((*itr).key >= tuple->key) {
			if (vioctr == 0) threshold = (*itr).key;
			vioctr++;
		}

	}

	if (vioctr == 0) threshold = -1;

	uint64_t expected;
	uint64_t lockBit = 0b100;
	// if too many violations
	// paper に too many の条件が無いので，自分で設定
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
				//ロックを取ってからビットを上げる
				expected = Table[tuple->key].tidword.load(memory_order_acquire);
				Table[tuple->key].tidword.store(expected | lockBit, memory_order_release);
				//-----
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
	
	if (vioctr != 0) {
		// Not in canonical mode. Restore.
		for (auto itr = CLL.begin() + (CLL.size() - vioctr); itr != CLL.end(); ++itr) {
			if ((*itr).mode) {
				//ビットを落としてからロックを解放する
				expected = Table[(*itr).key].tidword.load(memory_order_acquire);
				Table[(*itr).key].tidword.store(expected &(~lockBit), memory_order_release);
				//-----
				(*itr).lock->w_unlock();
			} else {
				(*itr).lock->r_unlock();
			}
		}
		//delete from CLL
		if (CLL.size() == vioctr) CLL.clear();
		else {
			CLL.erase(CLL.begin() + (CLL.size() - vioctr), CLL.end());
		}
	}

	// unconditional lock in canonical mode.
	for (auto itr = RLL.begin(); itr != RLL.end(); ++itr) {
		if ((*itr).key <= threshold) continue;
		if ((*itr).key < tuple->key) {
			if ((*itr).mode) {
				(*itr).lock->w_lock();
				//ロックを取ってからビットを上げる
				expected = Table[(*itr).key].tidword.load(memory_order_acquire);
				Table[(*itr).key].tidword.store(expected | lockBit, memory_order_release);
				//-----
			} else {
				(*itr).lock->r_lock();
			}
			CLL.push_back(*itr);
		} else {
			break;
		}
	}

	if (mode) {
		tuple->lock.w_lock();	
		//ロックを取ってからビットを上げる
		expected = Table[tuple->key].tidword.load(memory_order_acquire);
		Table[tuple->key].tidword.store(expected | lockBit, memory_order_release);
	}
	else {
		tuple->lock.r_lock();
	}

	CLL.push_back(LockElement(tuple->key, &(tuple->lock), mode));
	return;
}

void
Transaction::construct_rll()
{
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
		if (Table[(*itr).key].temp.load(memory_order_acquire) >= TEMP_THRESHOLD 
				|| (*itr).failed_verification) {
			RLL.push_back(LockElement((*itr).key, &(Table[(*itr).key].lock), false));
		}

		// maintain temprature p
		if ((*itr).failed_verification) {
			uint64_t expected, desired;
			for (;;) {
				expected = Table[(*itr).key].temp.load(memory_order_acquire);
RETRY_MAINTE_TEMP:
				if (expected == TEMP_MAX) break;
				if (Rnd[thid].next() % (1 << expected) == 0) {
					desired = expected + 1;
				} else break;
				if (Table[(*itr).key].temp.compare_exchange_strong(expected, desired, memory_order_acq_rel, memory_order_acquire)) break;
				else goto RETRY_MAINTE_TEMP;
			}
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
		lock(&Table[(*itr).key], true);
		if (this->status == TransactionStatus::aborted) {
			return false;
		}
	}

	for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
		uint64_t tmptidword = Table[(*itr).key].tidword.load(memory_order_acquire);
		if (((*itr).tidword >> 3) != tmptidword >> 3) {
			(*itr).failed_verification = true;
			this->status = TransactionStatus::aborted;
			return false;
		}

		// Silo プロトコル
		if (Table[(*itr).key].lock.counter.load(memory_order_acquire) == -1) {
			WriteElement *inW = searchWriteSet((*itr).key);
			//ロックが取られていて，自分が持ち主でないのなら abort
			if (inW == nullptr) {
				this->status = TransactionStatus::aborted;
				return false;
			}
		}
	}

	return true;
}

void
Transaction::abort()
{
	//unlock CLL
	unlockCLL();
	
	construct_rll();

	readSet.clear();
	writeSet.clear();
	return;
}

void
Transaction::unlockCLL()
{
	uint64_t expected;
	uint64_t lockBit = 0b100;

	for (auto itr = CLL.begin(); itr != CLL.end(); ++itr) {
		if ((*itr).mode) {
			// ビットを下げてからロックを解放する
			expected = Table[(*itr).key].tidword.load(memory_order_acquire);
			Table[(*itr).key].tidword.store(expected & (~lockBit), memory_order_release);
			(*itr).lock->w_unlock();
		}
		else {
			(*itr).lock->r_unlock();
		}
	}
	CLL.clear();
}

void
Transaction::writePhase()
{
	uint64_t tid_a = 0;
	uint64_t tid_b = 0;
	uint64_t tid_c = 0;
	uint64_t lockBit = 0b100;

	// calculates (a)
	// about readSet
	for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
		tid_a = max(tid_a, (*itr).tidword);
	}
	// about writeSet
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		tid_a = max(tid_a, Table[(*itr).key].tidword.load(memory_order_acquire));
	}

	//下位 3ビットは予約されている
	tid_a += 0b1000;

	//calculates (b)
	//larger than the worker's most recently chosen TID,
	//下位 3ビットは予約されている
	tid_b = ThRecentTID[thid].num + 0b1000;

	// calculates (c)
	tid_c = __atomic_load_n(&(ThLocalEpoch[thid].num), __ATOMIC_ACQUIRE) << 32;

	// compare a, b, c
	ThRecentTID[thid].num = max({tid_a, tid_b, tid_c});
	//更新用にロックビットを落としておく
	ThRecentTID[thid].num = ThRecentTID[thid].num & (~lockBit);

	//write (record, commit-tid)
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		//update nad down lockBit
		Table[(*itr).key].val.store((*itr).val, memory_order_release);
		Table[(*itr).key].tidword.store(ThRecentTID[thid].num, memory_order_release);
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

