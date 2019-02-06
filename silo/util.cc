#include <stdio.h>
#include <sys/syscall.h> // syscall(SYS_gettid),
#include <sys/time.h>
#include <sys/types.h> // syscall(SYS_gettid),
#include <unistd.h> // syscall(SYS_gettid),

#include <bitset>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <fstream>

#include "include/atomic_tool.hpp"
#include "include/common.hpp"
#include "include/procedure.hpp"
#include "include/transaction.hpp"
#include "include/tuple.hpp"

#include "../../include/debug.hpp"
#include "../../include/random.hpp"
#include "../../include/zipf.hpp"

bool
chkSpan(struct timeval &start, struct timeval &stop, long threshold)
{
	long diff = 0;
	diff += (stop.tv_sec - start.tv_sec) * 1000 * 1000 + (stop.tv_usec - start.tv_usec);
	if (diff > threshold) return true;
	else return false;
}

bool
chkClkSpan(uint64_t &start, uint64_t &stop, uint64_t threshold)
{
	uint64_t diff = 0;
	diff = stop - start;
	if (diff > threshold) return true;
	else return false;
}

bool
chkEpochLoaded()
{
	uint64_t nowepo = atomicLoadGE();
//全てのワーカースレッドが最新エポックを読み込んだか確認する．
	for (unsigned int i = 1; i < THREAD_NUM; ++i) {
		if (__atomic_load_n(&(ThLocalEpoch[i].obj), __ATOMIC_ACQUIRE) != nowepo) return false;
	}

	return true;
}


void 
displayDB() 
{
	Tuple *tuple;
	for (unsigned int i = 0; i < TUPLE_NUM; ++i) {
		tuple = &Table[i];
		cout << "------------------------------" << endl;	//-は30個
		cout << "key: " << tuple->key << endl;
		cout << "val: " << tuple->val << endl;
		cout << "TIDword: " << tuple->tidword.obj << endl;
		cout << "bit: " << tuple->tidword.obj << endl;
		cout << endl;
	}
}

void 
displayPRO(Procedure *pro) 
{
	for (unsigned int i = 0; i < MAX_OPE; ++i) {
   		cout << "(ope, key, val) = (";
		switch(pro[i].ope){
		case Ope::READ:
				cout << "READ";
				break;
			case Ope::WRITE:
				cout << "WRITE";
				break;
			default:
				break;
		}
   		cout << ", " << pro[i].key
   		<< ", " << pro[i].val << ")" << endl;
	}
}

void 
makeDB() 
{
	Tuple *tmp;
	Xoroshiro128Plus rnd;
	rnd.init();

	try {
		if (posix_memalign((void**)&Table, 64, (TUPLE_NUM) * sizeof(Tuple)) != 0) ERR;
	} catch (bad_alloc) {
		ERR;
	}

	for (unsigned int i = 0; i < TUPLE_NUM; ++i) {
		tmp = &Table[i];
		tmp->tidword.epoch = 1;
		tmp->tidword.latest = 1;
		tmp->tidword.lock = 0;
		tmp->key = i;
		tmp->val = rnd.next() % TUPLE_NUM;
	}

}

void 
makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd) 
{
	for (unsigned int i = 0; i < MAX_OPE; ++i) {
		if ((rnd.next() % 100) < RRATIO)
			pro[i].ope = Ope::READ;
		else
			pro[i].ope = Ope::WRITE;
		
		pro[i].key = rnd.next() % TUPLE_NUM;
		pro[i].val = rnd.next() % TUPLE_NUM;
	}
}

void 
makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd, FastZipf &zipf) {
	for (unsigned int i = 0; i < MAX_OPE; ++i) {
		if ((rnd.next() % 100) < RRATIO)
			pro[i].ope = Ope::READ;
	  else
			pro[i].ope = Ope::WRITE;

		pro[i].key = zipf() % TUPLE_NUM;
		pro[i].val = rnd.next() % TUPLE_NUM;
	}
}

void
setThreadAffinity(int myid)
{
  pid_t pid = syscall(SYS_gettid);
  cpu_set_t cpu_set;

	CPU_ZERO(&cpu_set);
	CPU_SET(myid % sysconf(_SC_NPROCESSORS_CONF), &cpu_set);

	if (sched_setaffinity(pid, sizeof(cpu_set_t), &cpu_set) != 0)
    ERR;
  return;
}

void
waitForReadyOfAllThread()
{
	unsigned int expected, desired;
	expected = Running.load(std::memory_order_acquire);
	do {
		desired = expected + 1;
	} while (!Running.compare_exchange_weak(expected, desired, std::memory_order_acq_rel, std::memory_order_acquire));

	while (Running.load(std::memory_order_acquire) != THREAD_NUM);
  return;
}
