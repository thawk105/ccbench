#include <atomic>

#include "include/lock.hpp"

#define xchg(...) __atomic_exchange_n(__VA_ARGS__)
#define cas(...) __atomic_compare_exchange_n(__VA_ARGS__)
#define atoload(arg) __atomic_load_n(arg, __ATOMIC_ACQUIRE)
#define atostore(...) __atomic_store_n(__VA_ARGS__)

#define SPIN 2400

MQL_RESULT
MQLock::reader_acquire(MQLnode *qnode, bool trylock)
{
	qnode->type = LMode::reader;
	MQLnode *p = __atomic_exchange_n(&tail, qnode, __ATOMIC_ACQ_REL);
	if (p == nullptr) {
		nreaders++;
		qnode->granted.store(true, std::memory_order_release);
		return finish_reader_acquire(qnode);
	}

handle_pred:
	// Make sure the previous cancelling requester has left
	MQL_suc_info load_suc_info;
	load_suc_info.obj = __atomic_load_n(&(p->suc_info.obj), __ATOMIC_ACQUIRE);
	while (load_suc_info.next == 0 && load_suc_info.stype == LMode::none) {
		load_suc_info.obj = __atomic_load_n(&(p->suc_info.obj), __ATOMIC_ACQUIRE);
	}

	if (__atomic_load_n(&(p->type), __ATOMIC_ACQUIRE) == LMode::reader) {
	}
	return MQL_RESULT::LockAquired;
}

MQL_RESULT
MQLock::finish_reader_acquire(MQLnode *qnode)
{
	MQL_suc_info expected_suc_info, desired_suc_info;
	expected_suc_info.obj = __atomic_load_n(&(qnode->suc_info.obj), __ATOMIC_ACQUIRE);
	for (;;) {
		desired_suc_info.obj = expected_suc_info.obj;
		desired_suc_info.busy = true;
		desired_suc_info.status = LStatus::granted;
		if (__atomic_compare_exchange_n(&(qnode->suc_info.obj), &(expected_suc_info.obj), desired_suc_info.obj, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) break;
	}
//	while (__atomic_load_n(&(qnode->suc_info.next), __ATOMIC_ACQUIRE) == &SuccessorLeaving);
	
	//if (__atomic_load_n(&tail, __ATOMIC_ACQUIRE) == qnode) {}


	return MQL_RESULT::LockAquired;
}

MQL_RESULT
MQLock::reader_cancel(MQLnode *qnode)
{
	return MQL_RESULT::LockAquired;
}

MQL_RESULT
MQLock::writer_acquire(MQLnode *qnode, bool trylock)
{
	qnode->type = LMode::writer;
	MQLnode *p = __atomic_exchange_n(&tail, qnode, __ATOMIC_ACQ_REL);
	if (p == nullptr) {
		qnode->granted.store(true, std::memory_order_release);
		return finish_writer_acquire(qnode);
	}
	return MQL_RESULT::LockAquired;
}

MQL_RESULT
MQLock::finish_writer_acquire(MQLnode *qnode)
{
	MQL_suc_info expected_suc_info, desired_suc_info;
	expected_suc_info.obj = __atomic_load_n(&(qnode->suc_info.obj), __ATOMIC_ACQUIRE);
	for (;;) {
		desired_suc_info.obj = expected_suc_info.obj;
		desired_suc_info.busy = true;
		desired_suc_info.status = LStatus::granted;
		if (__atomic_compare_exchange_n(&(qnode->suc_info.obj), &(expected_suc_info.obj), desired_suc_info.obj, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) break;
	}
	//while (__atomic_load_n(&(qnode->suc_info.next), __ATOMIC_ACQUIRE) == &SuccessorLeaving);

	return MQL_RESULT::LockAquired;
}

MQL_RESULT
MQLock::writer_cancel(MQLnode *qnode)
{
	return MQL_RESULT::LockAquired;
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
