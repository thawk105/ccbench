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
#include <xmmintrin.h>

extern bool chkSpan(struct timeval &start, struct timeval &stop, long threshold);
extern void displayDB();

using namespace std;

int Transaction::tread(unsigned int key)
{
	int read_value;

	//w
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		if ((*itr).key == key) return (*itr).val;
	}

	//r
	for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
		if ((*itr).key == key) return (*itr).val;
	}
	
	//(a) reads the TID word, spinning until the lock is clear
	uint64_t expected;
	uint64_t expected_check;
	uint64_t lockBit = 0b100;

	expected = Table[key].tidword.load(memory_order_acquire);
	//check if it is locked.
	//spinning until the lock is clear
RETRY:
	while (expected & lockBit) {
		expected = Table[key].tidword.load(memory_order_acquire);
	}
	
	//(b) checks whether the record is the latest version
	//今回は無し
	
	
	//(c) reads the data
	read_value = Table[key].val.load(memory_order_acquire);

	//(d) performs a memory fence
	//x
	
	//(e) checks the TID word again
	expected_check = Table[key].tidword.load(memory_order_acquire);
	if (expected != expected_check) {
		expected = expected_check;
		goto RETRY;
	}
	
	readSet.push_back(ReadElement(key, read_value, expected));
	return read_value;
}

void Transaction::twrite(unsigned int key, unsigned int val)
{
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		if ((*itr).key == key) {
			(*itr).val = val;
			return;
		}
	}

	writeSet.push_back(WriteElement(key, val));
	return;
}

bool Transaction::validationPhase()
{

	uint64_t lockBit = 0b100;

	//Phase 1 
	//ソートされた writeSet をロックする．
	sort(writeSet.begin(), writeSet.end());
	lockWriteSet();

	asm volatile("" ::: "memory");
	ThLocalEpoch[thid].store(GlobalEpoch.load(memory_order_acquire), memory_order_release);
	asm volatile("" ::: "memory");

	//Phase 2 下記条件を一つでも満たしたらアボート．
	//1. readSetのtidにreadPhaseでのアクセスから変更がある
	//2. または　最新バージョンではない
	//3. または　ロックされていて，このタプルがwriteSetに含まれていない（concurrent transaction が更新中であると考えられる．）
	for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
		//1
		//3bit シフトの理由：もしwriteSetに含まれるのであれば，ロックビットが上がっている(lockWriteSet() を行なっているので）．
		//readPhaseではロックビットが下がっている（ロックされていない）物を読み込んでいるので，
		//in readSet かつ in writeSet の時は一致しようがない．よって3ビットシフト

		uint64_t tmptidword = Table[(*itr).key].tidword.load(memory_order_acquire);
		if (((*itr).tidword >> 3) != (tmptidword) >> 3) {
			return false;
		}
		//2
		//今回は無し
		//3
		if (tmptidword & lockBit) {	//ロックされていて
			auto include = writeSet.begin();
			for (; include != writeSet.end(); ++include) {
				if ((*include).key == (*itr).key) return true;
			}
			return false;	//writeSetに含まれていない
		}
	}

	//Phase 3 writePhase()へ
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
	//It calculates the smallest number that is 
	//(a) larger than the TID of any record read or written by the transaction,
	//(b) larger than the worker's most recently chosen TID,
	//and (C) in the current global epoch.
	
	uint64_t tid_a = 0;
	uint64_t tid_b = 0;
	uint64_t tid_c = 0;
	uint64_t lockBit = 0b100;

	//calculates (a)
	//about readSet
	for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
		if (tid_a < (*itr).tidword) tid_a = (*itr).tidword;
	}
	//about writeSet
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		uint64_t tmp = Table[(*itr).key].tidword.load(memory_order_acquire);
		tid_a = max(tid_a, tmp);
	}
	//下位3ビットは予約されている．
	tid_a += 0b1000;
	
	//calculates (b)
	//larger than the worker's most recently chosen TID,
	tid_b = ThRecentTID[thid].num + 0b1000;	
	//下位3ビットは予約されている．

	//calculates (c)
	tid_c = ThLocalEpoch[thid] << 32;

	//compare a, b, c
	ThRecentTID[thid].num = max({tid_a, tid_b, tid_c});
	ThRecentTID[thid].num = ThRecentTID[thid].num & (~lockBit);

	//write(record, commit-tid)
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		//update and unlock
		Table[(*itr).key].val.store((*itr).val, memory_order_release);
		Table[(*itr).key].tidword.store(ThRecentTID[thid].num, memory_order_release);
	}

	readSet.clear();
	writeSet.clear();
	FinishTransactions[thid].num++;
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
