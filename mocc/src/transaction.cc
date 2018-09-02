#include <algorithm>
#include <cmath>
#include <random>
#include <stdio.h>

#include "include/debug.hpp"
#include "include/transaction.hpp"
#include "include/tuple.hpp"

using namespace std;

void
Transaction::begin(int thid)
{
	this->status = TransactionStatus::inFlight;
	this->thid = thid;

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
	//cout << "th " << thid << ": lock " << tuple->key << endl;

	vector<LockElement> violations;

	sort(CLL.begin(), CLL.end());
	vector<LockElement>::iterator cllviobgn = CLL.end();
	bool firstvio = true;
	// lock exists in CLL (Current Lock List)
	for (auto itr = CLL.begin(); itr != CLL.end(); ++itr) {
		// lock already exists in CLL 
		// 		&& its lock mode is equal to needed mode or it is stronger than needed mode.
		if ((*itr).key == tuple->key) {
			if (mode == (*itr).mode || mode < (*itr).mode) return;
		}

		//cout << (*itr).key << " " << tuple->key << endl;
		// collect violation
		if ((*itr).key >= tuple->key) {
			if (firstvio) {
				cllviobgn = itr;
				firstvio = false;
			}
			violations.push_back(*itr);
		}

	}

	uint64_t expected;
	uint64_t lockBit = 0b100;
	// if too many violations
	if ((CLL.size() / 2) < violations.size() && CLL.size() >= (MAX_OPE / 2)) {
		NNN;
		if (mode) {
			if (tuple->lock.w_trylock()) {
				//ロックを取ってからビットを上げる
				expected = Table[tuple->key % TUPLE_NUM].tidword.load(memory_order_acquire);
				Table[tuple->key % TUPLE_NUM].tidword.store(expected | lockBit, memory_order_release);
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
	else if (!violations.empty()) {
		NNN;
		// Not in canonical mode. Restore.
		for (auto itr = cllviobgn; itr != violations.end(); ++itr) {
			if ((*itr).mode) {
				//ビットを落としてからロックを解放する
				expected = Table[(*itr).key % TUPLE_NUM].tidword.load(memory_order_acquire);
				Table[(*itr).key % TUPLE_NUM].tidword.store(expected &(~lockBit), memory_order_release);
				//-----
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
	int rllctr = 0;

	// unconditional lock in canonical mode.
	for (auto itr = RLL.begin(); itr != RLL.end(); ++itr) {
		if ((*itr).key < tuple->key) {
			if ((*itr).mode) {
				(*itr).lock->w_lock();
				//ロックを取ってからビットを上げる
				expected = Table[(*itr).key % TUPLE_NUM].tidword.load(memory_order_acquire);
				Table[(*itr).key % TUPLE_NUM].tidword.store(expected | lockBit, memory_order_release);
				//-----
			} else {
				(*itr).lock->r_lock();
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
		tuple->lock.w_lock();	
	
		//ロックを取ってからビットを上げる
		expected = Table[tuple->key % TUPLE_NUM].tidword.load(memory_order_acquire);
		Table[tuple->key % TUPLE_NUM].tidword.store(expected | lockBit, memory_order_release);
	}
	else tuple->lock.r_lock();

	CLL.push_back(acquired_lock);

	// remove acquired lock from RLL
	for (auto itr = RLL.begin(); itr != RLL.end(); ++itr) {
		while (acquired_lock.key == (*itr).key) {
			RLL.erase(itr);
			if (itr == RLL.end()) break;
		}
		if (itr == RLL.end()) break;
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

		// maintain temprature p
		if ((*itr).failed_verification) {
			random_device rnd;
			uint64_t expected, desired;
			do {
				expected = Table[(*itr).key % TUPLE_NUM].temp.load(memory_order_acquire);
				double result = rnd() / (double)random_device::max();
				if (result == 0) {
					printf("temprature result is 0\n");
					exit(1);
				}
				if (result < 0 || result > 1) {
					printf("temprature result is ERR\n");
					exit(1);
				}
				if (result < 1.0 / pow(2, expected)) break;
				desired = expected + 1;
			} while (!Table[(*itr).key % TUPLE_NUM].temp.compare_exchange_strong(expected, desired, memory_order_acq_rel));
		}
	}

	sort(RLL.begin(), RLL.end());
		
	return;
}

bool
Transaction::commit()
{
	uint64_t lockBit = 0b100;

	// phase 1 lock write set.
	sort(writeSet.begin(), writeSet.end());
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		lock(&Table[(*itr).key % TUPLE_NUM], true);
		if (this->status == TransactionStatus::aborted) return false;
	}

	for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
		uint64_t tmptidword = Table[(*itr).key % TUPLE_NUM].tidword.load(memory_order_acquire);
		if (((*itr).tidword >> 3) != tmptidword >> 3) {
			//(*itr).failed_verification = true;
			this->status = TransactionStatus::aborted;
			//cout << "tid miss abort" << endl;
			return false;
		}

		// Silo プロトコル
		// 現 reader-writer lock ではロックの獲得に失敗することでしか，並行ワーカーがロックを獲得していることを知る術がない．
		// そこで，lockBitはここのためだけに採用する．
		// 基本的には reader-writer-lock で運用する．
		if (tmptidword & lockBit) {
			auto includeW = writeSet.begin();
			for (; includeW != writeSet.end(); ++includeW) {
				if ((*includeW).key == (*itr).key) break;
			}
			//ロックが取られていて，自分が持ち主でないのなら abort
			if (includeW == writeSet.end()) {
				this->status = TransactionStatus::aborted;
				//cout << "another acquired lock " << (*itr).key << " abort" << endl;
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
	AbortCounts[thid].num++;

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
			expected = Table[(*itr).key % TUPLE_NUM].tidword.load(memory_order_acquire);
			Table[(*itr).key % TUPLE_NUM].tidword.store(expected & (~lockBit), memory_order_release);
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
		tid_a = max(tid_a, Table[(*itr).key % TUPLE_NUM].tidword.load(memory_order_acquire));
	}

	//下位 3ビットは予約されている
	tid_a += 0b1000;

	//calculates (b)
	//larger than the worker's most recently chosen TID,
	//下位 3ビットは予約されている
	tid_b = ThRecentTID[thid].num + 0b1000;

	// calculates (c)
	tid_c = ThLocalEpoch[thid] << 32;

	// compare a, b, c
	ThRecentTID[thid].num = max({tid_a, tid_b, tid_c});
	//更新用にロックビットを落としておく
	ThRecentTID[thid].num = ThRecentTID[thid].num & (~lockBit);

	//write (record, commit-tid)
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		//update nad down lockBit
		Table[(*itr).key % TUPLE_NUM].val.store((*itr).val, memory_order_release);
		Table[(*itr).key % TUPLE_NUM].tidword.store(ThRecentTID[thid].num, memory_order_release);
	}

	unlockCLL();
	RLL.clear();
	readSet.clear();
	writeSet.clear();
	FinishTransactions[thid].num++;
}

void
Transaction::dispLock()
{
	cout << "th " << this->thid << ": ";
	for (auto itr = CLL.begin(); itr != CLL.end(); ++itr) {
		cout << (*itr).key << "(";
		if ((*itr).mode) cout << "w) ";
		else cout << "r) ";
	}
	cout << endl;
}
