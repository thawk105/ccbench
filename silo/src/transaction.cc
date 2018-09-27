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

WriteElement *
Transaction::searchWriteSet(unsigned int key)
{
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		if ((*itr).key == key) return &(*itr);
	}

	return nullptr;
}

ReadElement *
Transaction::searchReadSet(unsigned int key)
{
	for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
		if ((*itr).key == key) return &(*itr);
	}

	return nullptr;
}

int 
Transaction::tread(unsigned int key)
{
	int read_value;
	Tuple *tuple = &Table[key];

	//w
	WriteElement *inW = searchWriteSet(key);
	if (inW) return inW->val;

	//r
	ReadElement *inR = searchReadSet(key);
	if (inR) return inR->val;

	
	//(a) reads the TID word, spinning until the lock is clear
	Tidword expected;
	Tidword expected_check;

	expected.obj = __atomic_load_n(&(tuple->tidword.obj), __ATOMIC_ACQUIRE);
	//check if it is locked.
	//spinning until the lock is clear
RETRY:
	while (expected.lock) {
		expected.obj = __atomic_load_n(&(tuple->tidword.obj), __ATOMIC_ACQUIRE);
	}
	
	//(b) checks whether the record is the latest version
	//今回は無し
	
	
	//(c) reads the data
	read_value = tuple->val;

	//(d) performs a memory fence
	//x
	
	//(e) checks the TID word again
	expected_check.obj = __atomic_load_n(&(tuple->tidword.obj), __ATOMIC_ACQUIRE);
	if (expected != expected_check) {
		expected = expected_check;
		goto RETRY;
	}
	
	readSet.push_back(ReadElement(key, read_value, expected));
	return read_value;
}

void Transaction::twrite(unsigned int key, unsigned int val)
{
	WriteElement *inW = searchWriteSet(key);
	if (inW) {
		inW->val = val;
		return;
	}

	writeSet.push_back(WriteElement(key, val));
	return;
}

bool Transaction::validationPhase()
{


	//Phase 1 
	//ソートされた writeSet をロックする．
	sort(writeSet.begin(), writeSet.end());
	lockWriteSet();

	asm volatile("" ::: "memory");
	__atomic_store_n(&(ThLocalEpoch[thid].num), GlobalEpoch.load(memory_order_acquire), __ATOMIC_RELEASE);
	asm volatile("" ::: "memory");

	//Phase 2 下記条件を一つでも満たしたらアボート．
	//1. readSetのtidにreadPhaseでのアクセスから変更がある
	//2. または　最新バージョンではない
	//3. または　ロックされていて，このタプルがwriteSetに含まれていない（concurrent transaction が更新中であると考えられる．）
	
	Tidword tmptidword;
	for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
		//1
		//readPhaseではロックビットが下がっている（ロックされていない）物を読み込んでいるので，
		//in readSet かつ in writeSet の時は一致しようがない．よって3ビットシフト

		tmptidword.obj = __atomic_load_n(&(Table[(*itr).key].tidword.obj), __ATOMIC_ACQUIRE);
		if ((*itr).tidword.epoch != tmptidword.epoch || (*itr).tidword.tid != tmptidword.tid) {
			return false;
		}
		//2
		//今回は無し
		//3
		if (tmptidword.lock) {	//ロックされていて
			if (searchWriteSet((*itr).key)) return true;
			else return false;
		}
	}

	//Phase 3 writePhase()へ
	return true;
}

void Transaction::abort() 
{
	unlockWriteSet();

	readSet.clear();
	writeSet.clear();
}

void Transaction::writePhase()
{
	//It calculates the smallest number that is 
	//(a) larger than the TID of any record read or written by the transaction,
	//(b) larger than the worker's most recently chosen TID,
	//and (C) in the current global epoch.
	
	Tidword tid_a, tid_b, tid_c;

	//calculates (a)
	//about readSet
	for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
		if (tid_a < (*itr).tidword) tid_a = (*itr).tidword;
	}
	//about writeSet
	Tidword tmp;
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		tmp.obj = __atomic_load_n(&(Table[(*itr).key].tidword.obj), __ATOMIC_ACQUIRE);
		tid_a = max(tid_a, tmp);
	}
	//下位3ビットは予約されている．
	tid_a.tid++;
	
	//calculates (b)
	//larger than the worker's most recently chosen TID,
	tid_b = mrctid;
	tid_b.tid++;
	//下位3ビットは予約されている．

	//calculates (c)
	tid_c.epoch = ThLocalEpoch[thid].num;

	//compare a, b, c
	Tidword maxtid =  max({tid_a, tid_b, tid_c});
	maxtid.lock = 0;
	mrctid = maxtid;

	//write(record, commit-tid)
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		//update and unlock
		Table[(*itr).key].val = (*itr).val;
		__atomic_store_n(&(Table[(*itr).key].tidword.obj), maxtid.obj, __ATOMIC_RELEASE);
	}

	readSet.clear();
	writeSet.clear();
}

void Transaction::lockWriteSet()
{
	Tidword expected, desired;

	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		//lock
		expected.obj = __atomic_load_n(&(Table[(*itr).key].tidword.obj), __ATOMIC_ACQUIRE);
		for (;;) {
			if (expected.lock) {
				expected.obj = __atomic_load_n(&(Table[(*itr).key].tidword.obj), __ATOMIC_ACQUIRE);
			} else {
				desired = expected;
				desired.lock = 1;
				if (__atomic_compare_exchange_n(&(Table[(*itr).key].tidword.obj), &(expected.obj), desired.obj, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) break;
			}
		}
	}
}

void Transaction::unlockWriteSet()
{
	Tidword expected, desired;

	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		//unlock
		expected.obj = __atomic_load_n(&(Table[(*itr).key].tidword.obj), __ATOMIC_ACQUIRE);
		desired = expected;
		desired.lock = 0;
		__atomic_store_n(&(Table[(*itr).key].tidword.obj), desired.obj, __ATOMIC_RELEASE);
	}
}
