#include <atomic>
#include <cstdint>
#include <stdint.h>
#include <stdlib.h>
#include <sys/syscall.h> // syscall(SYS_gettid),
#include <sys/time.h>
#include <sys/types.h> // syscall(SYS_gettid),
#include <unistd.h> // syscall(SYS_gettid),
#include "include/debug.hpp"
#include "include/common.hpp"
#include "include/debug.hpp"
#include "include/procedure.hpp"
#include "include/random.hpp"
#include "include/timeStamp.hpp"
#include "include/transaction.hpp"
#include "include/tuple.hpp"

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

void 
makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd) {
	bool ronly = true;

	for (unsigned int i = 0; i < MAX_OPE; ++i) {
		if ((rnd.next() % 10) < RRATIO) {
			pro[i].ope = Ope::READ;
		} else {
			ronly = false;
			pro[i].ope = Ope::WRITE;
		}
		pro[i].key = rnd.next() % TUPLE_NUM;
		pro[i].val = rnd.next() % TUPLE_NUM;
	}
	
	if (ronly) pro[0].ronly = true;
	else pro[0].ronly = false;
}

void
makeDB(uint64_t *initial_wts)
{
	Tuple *tmp;
	Version *verTmp;
	Xoroshiro128Plus rnd;
	rnd.init();

	try {
		if (posix_memalign((void**)&Table, 64, TUPLE_NUM * sizeof(Tuple)) != 0) ERR;
		for (unsigned int i = 0; i < TUPLE_NUM; i++) {
			Table[i].latest = new Version();
		}
	} catch (bad_alloc) {
		ERR;
	}

	TimeStamp tstmp;
	tstmp.generateTimeStampFirst(0);
	*initial_wts = tstmp.ts;
	for (unsigned int i = 0; i < TUPLE_NUM; i++) {
		tmp = &Table[i];
		tmp->min_wts = tstmp.ts;
		tmp->key = i;
		tmp->gClock.store(0, std::memory_order_release);
		verTmp = tmp->latest.load();
		verTmp->wts.store(tstmp.ts, std::memory_order_release);;
		verTmp->status.store(VersionStatus::committed, std::memory_order_release);
		verTmp->key = i;
		verTmp->val = rnd.next() % (TUPLE_NUM * 10);
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
