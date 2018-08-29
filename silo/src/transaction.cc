#include "include/transaction.hpp"
#include "include/common.hpp"
#include "include/debug.hpp"
#include <sys/time.h>
#include <stdio.h>
#include <fstream>
#include <string>
#include <bitset>
#include <sstream>
#include <algorithm>

extern bool chkSpan(struct timeval &start, struct timeval &stop, long threshold);
extern void displayDB();

using namespace std;

int Transaction::tread(int key)
{
	int read_value;

	//w
	auto includeW = writeSet.find(key);
	if (includeW != writeSet.end()) {
		return includeW->second;
	}

	//r
	auto includeR = readSet.find(key);
	if (includeR != readSet.end()) {
		return Table[includeR->first % TUPLE_NUM].val;
	}
	
	//(a) reads the TID word, spinning until the lock is clear
	uint64_t expected;
	uint64_t expected_check;
	uint64_t lockBit = 0b100;

	expected = Table[key % TUPLE_NUM].tidword.load(memory_order_acquire);
	//check if it is locked.
	//spinning until the lock is clear
RETRY:
	while (expected & lockBit) {
		expected = Table[key % TUPLE_NUM].tidword.load(memory_order_acquire);
	}
	
	//(b) checks whether the record is the latest version
	//今回は無し
	
	
	//(c) reads the data
	read_value = Table[key % TUPLE_NUM].val.load(memory_order_acquire);
	readSet[key] = expected;

	//(d) performs a memory fence
	//x
	
	//(e) checks the TID word again
	expected_check = Table[key % TUPLE_NUM].tidword.load(memory_order_acquire);
	if (expected != expected_check) {
		expected = expected_check;
		goto RETRY;
	}
	
	return read_value;
}

void Transaction::twrite(int key, int val)
{
	writeSet[key] = val;
}

bool Transaction::validationPhase()
{

	uint64_t lockBit = 0b100;

	//Phase 1 
	//ソートされた writeSet をロックする．
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

		uint64_t tmptidword = Table[itr->first % TUPLE_NUM].tidword.load(memory_order_acquire);
		if ((itr->second >> 3) != (tmptidword) >> 3) {
			unlockWriteSet();
			return false;
		}
		//2
		//今回は無し
		//3
		if (tmptidword & lockBit) {	//ロックされていて
			auto include = writeSet.find(itr->first);
			if (include == writeSet.end()) {
				unlockWriteSet();
				return false;	//writeSetに含まれていない
			}
		}
	}

	//Phase 3 writePhase()へ
	

	return true;
}

void Transaction::abort() 
{
	AbortCounts[thid].num++;
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
		if (tid_a < itr->second) tid_a = itr->second;
	}
	//about writeSet
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		uint64_t tmp = Table[itr->first % TUPLE_NUM].tidword.load(memory_order_acquire);
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
		Table[itr->first % TUPLE_NUM].val.store(itr->second, memory_order_release);
		Table[itr->first % TUPLE_NUM].tidword.store(ThRecentTID[thid].num, memory_order_release);
	}
}

void Transaction::lockWriteSet()
{
	uint64_t expected;
	uint64_t lockBit = 0b100;

	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		//lock
		for (;;) {
			expected = Table[itr->first % TUPLE_NUM].tidword.load(memory_order_acquire);
			if (!(expected & lockBit)) {
				if (Table[itr->first % TUPLE_NUM].tidword.compare_exchange_strong(expected, expected | lockBit, memory_order_acq_rel)) break;
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
		expected = Table[itr->first % TUPLE_NUM].tidword.load(memory_order_acquire);
		Table[itr->first % TUPLE_NUM].tidword.store(expected & (~lockBit), memory_order_release);
	}
}
