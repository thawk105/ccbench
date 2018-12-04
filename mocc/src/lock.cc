#include <atomic>

#include "include/common.hpp"
#include "include/lock.hpp"
#include "include/tsc.hpp"

#define xchg(...) __atomic_exchange_n(__VA_ARGS__)
#define cas(...) __atomic_compare_exchange_n(__VA_ARGS__)
#define atoload(arg) __atomic_load_n(arg, __ATOMIC_ACQUIRE)
#define atostore(...) __atomic_store_n(__VA_ARGS__)

#define SPIN 2400

MQL_RESULT
MQLock::reader_acquire(uint16_t nodenum, bool trylock)
{
	uint64_t spinstart, spinstop;
	MQLnode *qnode = &MQL_node_list[nodenum];
	qnode->type = LMode::reader;
	// initialize
	qnode->suc_info.next = 0;
	qnode->suc_info.busy = false;
	qnode->suc_info.stype = LMode::reader;
	qnode->suc_info.status = LStatus::waiting;

	uint16_t pretail = tail.exchange(nodenum);
	MQLnode *p = &MQL_node_list[pretail];

	if (pretail == 0) {
		nreaders++;
		qnode->granted.store(true, std::memory_order_release);
		return finish_reader_acquire(nodenum);
	}

handle_pred:
	// Make sure the previous cancelling requester has left
	MQL_suc_info load_suc_info;
	load_suc_info.obj = __atomic_load_n(&(p->suc_info.obj), __ATOMIC_ACQUIRE);
	while (!(load_suc_info.next == 0 && load_suc_info.stype == LMode::none)) {
		load_suc_info.obj = __atomic_load_n(&(p->suc_info.obj), __ATOMIC_ACQUIRE);
	}

	if (__atomic_load_n(&(p->type), __ATOMIC_ACQUIRE) == LMode::reader) {
		// predecessor is reader
		MQL_suc_info expected, desired, latest;
		expected.next = 0;
		expected.busy = false;
		expected.stype = LMode::none;
		expected.status = LStatus::waiting;
		desired.next = nodenum;
		desired.busy = false;
		desired.stype = LMode::reader;
		desired.status = LStatus::waiting;
		__atomic_compare_exchange_n(&(p->suc_info.obj), &expected.obj, desired.obj, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
		
		latest.obj = __atomic_load_n(&(p->suc_info.obj), __ATOMIC_ACQUIRE);
		if (latest.busy == false && latest.stype == LMode::none && latest.status == LStatus::waiting) {
			// if me.granted becomes True before timing out
			// 		return finish_reader_acquire
			// return cancel_reader_lock
			if (trylock) {
				spinstart = rdtsc();
				while(qnode->granted.load(std::memory_order_acquire) != true) {
					spinstop = rdtsc();
					if (chkClkSpan(spinstart, spinstop, LOCK_TIMEOUT_US * CLOCK_PER_US))
						return cancel_reader_lock(nodenum);
				}
				return finish_reader_acquire(nodenum);
			}
			else {
				while (qnode->granted.load(std::memory_order_acquire) != true);
				return finish_reader_acquire(nodenum);
			}

		}

		if (latest.status == LStatus::leaving) {
			MQL_suc_info expected, desired;
			expected.obj = __atomic_load_n(&(p->suc_info.obj), __ATOMIC_ACQUIRE);
			for (;;) {
				desired = expected;
				desired.next = nodenum;
				if (__atomic_compare_exchange_n(&(p->suc_info.obj), &expected.obj, desired.obj, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) break;
			}

			qnode->prev.store(pretail, std::memory_order_acquire);
			while (qnode->prev.load(std::memory_order_acquire) != pretail);
			pretail = qnode->prev.exchange(0);
			if (pretail == 1) {
				while (qnode->granted.load(std::memory_order_acquire) != true);
				return finish_reader_acquire(nodenum);
			}
			goto handle_pred; // p must point to a valid predecessor;
		}
		else {
			MQL_suc_info expected, desired;
			expected.obj = __atomic_load_n(&(p->suc_info.obj), __ATOMIC_ACQUIRE);
			for (;;) {
				desired = expected;
				desired.next = 3; // index 3 mean NoSuccessor
				if (__atomic_compare_exchange_n(&(p->suc_info.obj), &expected.obj, desired.obj, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) break;
			}
			nreaders++;
			qnode->granted.store(true, std::memory_order_release);
			return finish_reader_acquire(nodenum);
		}
	}
	else {	// predecessor is a writer, spin-wait with timeout
			MQL_suc_info expected, desired;
			expected.obj = __atomic_load_n(&(p->suc_info.obj), __ATOMIC_ACQUIRE);
			for (;;) {
				desired = expected;
				desired.stype = LMode::reader;
				desired.next = nodenum;
				if (__atomic_compare_exchange_n(&(p->suc_info.obj), &expected.obj, desired.obj, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) break;
			}

			if (qnode->prev.exchange(pretail) == 1) { // index 1 mean Acquired
				while (qnode->granted.load(std::memory_order_acquire) != true);
				return finish_reader_acquire(nodenum);
			}
			// else if me.granted is False after timeout
			// 		return cancel_reader_lock
			if (trylock) {
				spinstart = rdtsc();
				while(qnode->granted.load(std::memory_order_acquire) != true) {
					spinstop = rdtsc();
					if (chkClkSpan(spinstart, spinstop, LOCK_TIMEOUT_US * CLOCK_PER_US))
						return cancel_reader_lock(nodenum);
				}
				return finish_reader_acquire(nodenum);
			}
			else {
				while (qnode->granted.load(std::memory_order_acquire) != true);
				return finish_reader_acquire(nodenum);
			}
	}
}

MQL_RESULT
MQLock::finish_reader_acquire(uint16_t nodenum)
{
	MQLnode *me = &MQL_node_list[nodenum];
	MQL_suc_info expected, desired;
	expected.obj = __atomic_load_n(&(me->suc_info.obj), __ATOMIC_ACQUIRE);
	for (;;) {
		desired = expected;
		desired.busy = true;
		desired.status = LStatus::granted;
		if (__atomic_compare_exchange_n(&(me->suc_info.obj), &expected.obj, desired.obj, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) break;
	}

	expected.obj = __atomic_load_n(&(me->suc_info.obj), __ATOMIC_ACQUIRE);
	return MQL_RESULT::LockAcquired;
}

MQL_RESULT
MQLock::cancel_reader_lock(uint16_t nodenum)
{
	return MQL_RESULT::LockCancelled;
}

void
RWLock::r_lock()
{
	int expected, desired;
	for (;;) {
		expected = counter.load(std::memory_order_acquire);
RETRY_R_LOCK:
		if (expected != -1) desired = expected + 1;
		else {
			continue;
		}
		if (counter.compare_exchange_strong(expected, desired, std::memory_order_acq_rel, std::memory_order_acquire)) break;
		else goto RETRY_R_LOCK;
	}
}

bool
RWLock::r_trylock()
{
	int expected, desired;
	for (;;) {
		expected = counter.load(std::memory_order_acquire);
RETRY_R_TRYLOCK:
		if (expected != -1) desired = expected + 1;
		else return false;

		if (counter.compare_exchange_strong(expected, desired, std::memory_order_acq_rel, std::memory_order_acquire)) return true;
		else goto RETRY_R_TRYLOCK;
	}
}

void 
RWLock::r_unlock()
{
	counter--;
}

void
RWLock::w_lock()
{
	int expected, desired(-1);
	for (;;) {
		expected = counter.load(std::memory_order_acquire);
RETRY_W_LOCK:
		if (expected != 0) continue;

		if (counter.compare_exchange_strong(expected, desired, std::memory_order_acq_rel, std::memory_order_acquire)) break;
		else goto RETRY_W_LOCK;
	}
}

bool
RWLock::w_trylock()
{
	int expected, desired(-1);
	for (;;) {
		expected = counter.load(std::memory_order_acquire);
RETRY_W_TRYLOCK:
		if (expected != 0) return false;

		if (counter.compare_exchange_strong(expected, desired, std::memory_order_acq_rel, std::memory_order_acquire)) return true;
		else goto RETRY_W_TRYLOCK;
	}
}

void
RWLock::w_unlock()
{
	counter++;
}

bool 
RWLock::upgrade()
{
	int expected = 1;
	return counter.compare_exchange_weak(expected, -1, std::memory_order_acq_rel);
}
