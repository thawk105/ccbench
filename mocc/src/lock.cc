#include <atomic>

#include "include/common.hpp"
#include "include/lock.hpp"
#include "include/tsc.hpp"

#define xchg(...) __atomic_exchange_n(__VA_ARGS__)
#define cas(...) __atomic_compare_exchange_n(__VA_ARGS__)
#define atoload(arg) __atomic_load_n(arg, __ATOMIC_ACQUIRE)
#define atostore(...) __atomic_store_n(__VA_ARGS__)

#define SPIN 2400

MQLMetaInfo
MQLNode::atomicLoadSucInfo()
{
	MQLMetaInfo load;
 load.obj	= __atomic_load_n(&sucInfo.obj, __ATOMIC_ACQUIRE);
 return load;
}

bool
MQLNode::atomicCASSucInfo(MQLMetaInfo expected, MQLMetaInfo desired)
{
	return __atomic_compare_exchange_n(&sucInfo.obj, &expected.obj, desired.obj, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
}

void 
MQLMetaInfo::init(bool busy, LockMode stype, LockStatus status, uint32_t next)
{
	this->busy = busy;
	this->stype = stype;
	this->status = status;
	this->next = next;
}

bool
MQLMetaInfo::atomicLoadBusy()
{
	MQLMetaInfo expected;
	expected.obj = __atomic_load_n(&obj, __ATOMIC_ACQUIRE);
	return expected.busy;
}

void
MQLMetaInfo::atomicStoreBusy(bool newbusy)
{
	MQLMetaInfo expected, desired;
	expected.obj = __atomic_load_n(&obj, __ATOMIC_ACQUIRE);
	for (;;) {
		desired = expected;
		desired.busy = newbusy;
		if (__atomic_compare_exchange_n(&obj, &expected.obj, desired.obj, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) break;
	}
}

LockMode 
MQLMetaInfo::atomicLoadStype()
{
	MQLMetaInfo expected;
	expected.obj = __atomic_load_n(&obj, __ATOMIC_ACQUIRE);
	return expected.stype;
}

void
MQLMetaInfo::atomicStoreStype(LockMode newlockmode)
{
	MQLMetaInfo expected, desired;
	expected.obj = __atomic_load_n(&obj, __ATOMIC_ACQUIRE);
	for (;;) {
		desired = expected;
		desired.stype = newlockmode;
		if (__atomic_compare_exchange_n(&obj, &expected.obj, desired.obj, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) break;
	}
}

LockStatus
MQLMetaInfo::atomicLoadStatus()
{
	MQLMetaInfo expected;
	expected.obj = __atomic_load_n(&obj, __ATOMIC_ACQUIRE);
	return expected.status;
}

void
MQLMetaInfo::atomicStoreStatus(LockStatus newstatus)
{
	MQLMetaInfo expected, desired;
	expected.obj = __atomic_load_n(&obj, __ATOMIC_ACQUIRE);
	for (;;) {
		desired = expected;
		desired.status = newstatus;
		if (__atomic_compare_exchange_n(&obj, &expected.obj, desired.obj, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) break;
	}
}

uint32_t 
MQLMetaInfo::atomicLoadNext()
{
	MQLMetaInfo expected;
	expected.obj = __atomic_load_n(&obj, __ATOMIC_ACQUIRE);
	return expected.next;
}

void
MQLMetaInfo::atomicStoreNext(uint32_t newnext)
{
	MQLMetaInfo expected, desired;
	expected.obj = __atomic_load_n(&obj, __ATOMIC_ACQUIRE);
	for (;;) {
		desired = expected;
		desired.next = newnext;
		if (__atomic_compare_exchange_n(&obj, &expected.obj, desired.obj, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) break;
	}
}

bool
MQLMetaInfo::atomicCASNext(uint32_t oldnext, uint32_t newnext)
{
	MQLMetaInfo expected, desired;
	expected.obj = __atomic_load_n(&obj, __ATOMIC_ACQUIRE);
	expected.next = oldnext;
	desired = expected;
	desired.next = newnext;
	return __atomic_compare_exchange_n(&obj, &expected.obj, desired.obj, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
}

MQL_RESULT
MQLock::acquire_reader_lock(uint32_t me, bool trylock)
{
	MQLNode *qnode = &MQLNodeList[me];
	// initialize
	qnode->sucInfo.init(false, LockMode::None, LockStatus::Waiting, (uint32_t)SentinelValue::None);

	uint32_t p = tail.exchange(me);
	if (p == (uint32_t)SentinelValue::None) {
		nreaders++;
		qnode->granted.store(true, std::memory_order_release);
		return finish_acquire_reader_lock(me);
	}

	MQLNode *pred = &MQLNodeList[p];
	// haven't set pred.next.id yet, safe to dereference pred
	if (pred->type.load(std::memory_order_acquire) == LockMode::Reader)
		return acquire_reader_lock_check_reader_pred(me, p, trylock);
	return acquire_reader_lock_check_writer_pred(me, p, trylock);
}

MQL_RESULT
MQLock::finish_acquire_reader_lock(uint32_t me)
{
	MQLNode *qnode = &MQLNodeList[me];
	qnode->sucInfo.atomicStoreBusy(true);
	qnode->sucInfo.atomicStoreStatus(LockStatus::Granted);

	// spin until me.next is not SuccessorLeaving
	while (qnode->sucInfo.atomicLoadNext() == (uint32_t)SentinelValue::SuccessorLeaving);

	// if the lock tail now still points to me, truly no one is there, we're done
	if (tail.load(std::memory_order_acquire) == me) {
		// no successor if tail still points to me
		qnode->sucInfo.atomicStoreBusy(false);
		return MQL_RESULT::Acquired;
	}

	// note that the successor can't cancel now, ie me.next pointer is stable
	while (qnode->sucInfo.atomicLoadNext() == (uint32_t)SentinelValue::None);

	uint32_t sucnum = qnode->sucInfo.atomicLoadNext();
	MQLNode *suc = &MQLNodeList[sucnum];
	if (sucnum == (uint32_t)SentinelValue::None || suc->type.load(std::memory_order_acquire) == LockMode::Writer) { 
		qnode->sucInfo.atomicStoreBusy(false);
		return MQL_RESULT::Acquired;
	}

	// successor might be cancelling, in which case it'd xchg xchg me.next.id to None
	// it's also possible that my cancelling writer successor is about to give me a new 
	// reader successor, in this case my cancelling successor will realize that I already
	// have the lock and try to wake up the new successor directly also by trying to change
	// me.next.id to None (the new successor might spin forever if its timeout is 
	// Never and the cancelling successor didn't wake it up).
	//
	// if not CAS(me.next, successor, None)
	if (!qnode->sucInfo.atomicCASNext(sucnum, (uint32_t)SentinelValue::None)) {
		qnode->sucInfo.atomicStoreBusy(false);
		return MQL_RESULT::Acquired;
	}
	
	// if me.status is Granted and me.stype is None:
	// successor might have seen me in leaving state, it'll wait for me in that case
	// in this case, the successor saw me in leaving state and didn't register as a reader.
	// ie successor was acquiring
	while (suc->prev.load(std::memory_order_acquire) != me);
	if (suc->prev.compare_exchange_strong(me, (uint32_t)SentinelValue::Acquired, std::memory_order_acq_rel, std::memory_order_acquire)) {
		nreaders++;
		suc->granted.store(true, std::memory_order_release);
		// make sure I know when releasing no need to wait
		qnode->sucInfo.atomicStoreNext((uint32_t)SentinelValue::None);
	} 
	else if (qnode->sucInfo.atomicLoadStype() == LockMode::Reader) {
		for (;;) {
			while (suc->prev.load(std::memory_order_acquire) == me);
			if (suc->prev.compare_exchange_strong(me, (uint32_t)SentinelValue::Acquired, std::memory_order_acq_rel, std::memory_order_acquire)) {
				nreaders++;
				suc->granted.store(true, std::memory_order_release);
				qnode->sucInfo.atomicStoreNext((uint32_t)SentinelValue::None);
				break;
			}
		}
	}

	qnode->sucInfo.atomicStoreBusy(false);
	return MQL_RESULT::Acquired;
}

MQL_RESULT
MQLock::acquire_reader_lock_check_reader_pred(uint32_t me, uint32_t pred, bool trylock) 
{
	uint64_t spinstart, spinstop;
	uint32_t pretail;
	MQLNode *qnode = &MQLNodeList[me];
	MQLNode *p = &MQLNodeList[pred];
check_pred:
	// wait for the previous canceling dude to leave
	while (!(p->sucInfo.atomicLoadNext() == (uint32_t)SentinelValue::None && p->sucInfo.atomicLoadStype() == LockMode::None));

	MQLMetaInfo expected, desired;
	expected.init(false, LockMode::None, LockStatus::Waiting, (uint32_t)SentinelValue::None);
	desired.init(false, LockMode::Reader, LockStatus::Waiting, me);
	__atomic_compare_exchange_n(&(p->sucInfo.obj), &expected.obj, desired.obj, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
		
	if (p->sucInfo.atomicLoadBusy() == false 
				&& p->sucInfo.atomicLoadStype() == LockMode::None 
				&& p->sucInfo.atomicLoadStatus() == LockStatus::Waiting) {
		// link_pred(pred, me)
		p->sucInfo.atomicStoreNext(me);
			// if me.granted becomes True before timing out
			// 		return finish_reader_acquire
			// return cancel_reader_lock
			if (trylock) {
				spinstart = rdtsc();
				while(qnode->granted.load(std::memory_order_acquire) != true) {
					spinstop = rdtsc();
					if (chkClkSpan(spinstart, spinstop, LOCK_TIMEOUT_US * CLOCK_PER_US))
						return cancel_reader_lock(me);
				}
				return finish_acquire_reader_lock(me);
			}
			else {
				while (qnode->granted.load(std::memory_order_acquire) != true);
				return finish_acquire_reader_lock(me);
			}

		}

	// Failed cases
	if (p->sucInfo.atomicLoadStatus() == LockStatus::Leaving) {
		// don't set pred.next.successor_class here
		p->sucInfo.atomicStoreNext(me);

		// if pred did cancel, it will give me a new pred;
		// if it got the lock it will wake me up
			qnode->prev.store(pred, std::memory_order_release);
			while (qnode->prev.load(std::memory_order_acquire) != pred);
			// consume it and retry
			pretail = qnode->prev.exchange((uint32_t)SentinelValue::None);
			if (pretail == (uint32_t)SentinelValue::Acquired) {
				while (qnode->granted.load(std::memory_order_acquire) != true);
				return finish_release_reader_lock(me);
			}
			p = &MQLNodeList[pretail];
			if (p->type.load(std::memory_order_acquire) == LockMode::Writer) 
				return acquire_reader_lock_check_writer_pred(me, pretail, trylock);
		goto check_pred; // p must point to a valid predecessor;
		}
		else {
			// pred is granted - might be a direct grant or grant in the leaving process
			// I didn't register, pred won't wake me up, but if pred is leaving_granted,
			// we need to tell it not to poke me in its finish-acquire call.
			// For direct_granted,
			// also set its next.id to None so it knows that there's no need to wait and 
			// examine successor upon release. This also covers the 
			// case when pred.next.flags has Busy set.
			p->sucInfo.atomicStoreNext((uint32_t)SentinelValue::None);
			nreaders++;
			qnode->granted.store(true, std::memory_order_release);
			return finish_release_reader_lock(me);
		}
}

MQL_RESULT
MQLock::cancel_reader_lock(uint32_t me)
{
	MQLNode *qnode = &MQLNodeList[me];
	uint32_t pred = qnode->prev.exchange((uint32_t)SentinelValue::None); 
	// prevent from cancelling
	
	if (pred == (uint32_t)SentinelValue::Acquired) {
		while (qnode->granted.load(std::memory_order_acquire) != true);
		return finish_acquire_reader_lock(me);
	}

	// make sure successor can't leave, unless it tried to leave first
	qnode->sucInfo.atomicStoreStatus(LockStatus::Leaving);
	while (qnode->sucInfo.atomicLoadNext() == (uint32_t)SentinelValue::SuccessorLeaving);

	if (MQLNodeList[qnode->prev.load(std::memory_order_acquire)].type.load(std::memory_order_acquire)
			== LockMode::Reader) {
		return cancel_reader_lock_with_reader_pred(me, pred);
	}
	return cancel_reader_lock_with_writer_pred(me, pred);
}

MQL_RESULT
MQLock::cancel_reader_lock_with_writer_pred(uint32_t me, uint32_t pred)
{
retry:
	MQLNode *qnode = &MQLNodeList[me];
	MQLNode *p = &MQLNodeList[pred];
	// wait for the cancelling pred to finish relink
	// spin until pred.next is me and pred.stype is Reader
	// pred is a writer, so I can go as long as it's not also leaving (cancelling or releasing)
	for (;;) {
		// must take a copy first
		MQLMetaInfo eflags = p->atomicLoadSucInfo();
		if (eflags.status == LockStatus::Leaving) {
			// must wait for pred to give me a new pred (or wait to be waken up?)
			// pred should give me a new pred, after its CAS trying to pass me the lock failed
			qnode->prev.store(pred, std::memory_order_release);
			pred = qnode->prev.exchange((uint32_t)SentinelValue::None);
			if (pred == (uint32_t)SentinelValue::None 
					|| pred == (uint32_t)SentinelValue::Acquired)  {
				while (qnode->granted.load(std::memory_order_acquire) != true);
				return finish_acquire_reader_lock(me);
			}
			else {
				// make sure successor can't leave, unless it tried to leave first
				qnode->sucInfo.atomicStoreStatus(LockStatus::Leaving);
				while (qnode->sucInfo.atomicLoadNext() == (uint32_t)SentinelValue::SuccessorLeaving);
				// (tanabe) pred may be changed to new value at L:318, so not use p but use full path (MQLNodeList[pred]...)
				if (MQLNodeList[pred].type.load(std::memory_order_acquire)
						== LockMode::Reader) 
					// (tanabe) if it gets new reader pred, it executes cancel for reader
					return cancel_reader_lock_with_reader_pred(me, pred);
				// (tanabe) get new pred because old pred left.
				pred = qnode->prev.load(std::memory_order_acquire);
				goto retry;
			}	
		}
		else if (eflags.busy == true) {
			qnode->prev.store(pred, std::memory_order_release);
			while (qnode->granted.load(std::memory_order_acquire) != true);
			return finish_acquire_reader_lock(me);
		}
		// try to tell pred I'm leaving
		p = &MQLNodeList[pred];
		MQLMetaInfo expected, desired;
		expected = eflags;
		expected.next = me;
		desired = expected;
		desired.next = (uint32_t)SentinelValue::SuccessorLeaving;
		if (p->atomicCASSucInfo(expected, desired)) break;
	}

	// pred now has SuccessorLeaving on its next.id, it won't try to wake me up during release
	// now link the new successor and pred
	if (qnode->sucInfo.atomicLoadNext() == (uint32_t)SentinelValue::None
			&& tail.compare_exchange_strong(me, pred, std::memory_order_acq_rel, std::memory_order_acquire)) {
		p = &MQLNodeList[pred];
		p->sucInfo.atomicStoreStype(LockMode::None);
		p->sucInfo.atomicStoreNext((uint32_t)SentinelValue::None);
		return MQL_RESULT::Cancelled;
	}

	cancel_reader_lock_relink(pred, me);
	return MQL_RESULT::Cancelled;
}

MQL_RESULT
MQLock::cancel_reader_lock_with_reader_pred(uint32_t me, uint32_t pred)
{
retry:
	MQLNode *qnode = &MQLNodeList[me];
	MQLNode *p = &MQLNodeList[pred];
	// now successor can't attach to me assuming I'm waiting or has already done so.
	// CAS out of pred.next (including id and flags)
	// wait for the canceling pred to finish the relink
	// spin until pred.stype is Reader and (pred.next is me or pred.next is NoSuccessor
	// only want to put SuccessorLeaving in the id field
	MQLMetaInfo expected, desired;
	expected.init(false, LockMode::Reader, LockStatus::Waiting, me);
	desired.init(false, LockMode::Reader, LockStatus::Waiting, (uint32_t)SentinelValue::SuccessorLeaving);
 	if (!p->atomicCASSucInfo(expected, desired)) {
		// Note: we once registered after pred as a reader successor (still are), so if
		// pred happens to get the lock, it will wake me up seeing its reader_successor set
		if (p->sucInfo.atomicLoadStatus() == LockStatus::Granted) {
			// pred will in its finish-acquire-reader() wake me up.
			// pred already should alredy have me on its next.id and has reader successor class,
			// (the CAS loop in the "acquire" block).
			// this also covers the case when pred.next.flags has busy set.
			qnode->prev.store(pred, std::memory_order_acquire);
			while (qnode->granted.load(std::memory_order_acquire) != true);
			return finish_acquire_reader_lock(me);
		}
		else {
			// pred is trying to leave, wait for a new pred or being waken up
			// pred has higher priority to leave, and it should already have me on its next.id
			qnode->prev.store(pred, std::memory_order_acquire);
			while (qnode->prev.load(std::memory_order_acquire) != pred);
			// consume it and retry
			pred = qnode->prev.exchange((uint32_t)SentinelValue::None);
			if (pred == (uint32_t)SentinelValue::Acquired) {
				while (qnode->granted.load(std::memory_order_acquire) != true);
				return finish_acquire_reader_lock(me);
			}
			MQLNode *p = &MQLNodeList[pred];
			if (p->type.load(std::memory_order_acquire) == LockMode::Writer)
				return cancel_reader_lock_with_writer_pred(me, pred);
			goto retry;
		}
	}
	else {
		// at this point pred will be waiting for a new successor if it decides
		// to move and successor will be waiting for a new pred
		if (qnode->sucInfo.atomicLoadStype() == LockMode::None && tail.compare_exchange_strong(me, pred, std::memory_order_acq_rel, std::memory_order_acquire)) {
			// newly arriving successor for this pred will wait
			// for the SuccessorLeaving mark to go away before trying the CAS
			p->sucInfo.atomicStoreStype(LockMode::None);
			p->sucInfo.atomicStoreNext((uint32_t)SentinelValue::None);
			return MQL_RESULT::Cancelled;
		}
		cancel_reader_lock_relink(pred, me);
	}
	return MQL_RESULT::Cancelled;
}

MQL_RESULT
MQLock::cancel_reader_lock_relink(uint32_t pred, uint32_t me)
{
	return MQL_RESULT::Cancelled;
}

MQL_RESULT
MQLock::acquire_reader_lock_check_writer_pred(uint32_t me, uint32_t pred, bool trylock) 
{
	return MQL_RESULT::Acquired;
}

MQL_RESULT
MQLock::release_reader_lock(uint32_t me)
{
	return MQL_RESULT::Acquired;
}

MQL_RESULT
MQLock::finish_release_reader_lock(uint32_t me)
{
	return MQL_RESULT::Acquired;
}

MQL_RESULT
MQLock::acquire_writer_lock(uint32_t me, bool trylock)
{
	return MQL_RESULT::Acquired;
}

MQL_RESULT
MQLock::release_writer_lock(uint32_t me)
{
	return MQL_RESULT::Acquired;
}

MQL_RESULT
MQLock::cancel_writer_lock(uint32_t me)
{
	return MQL_RESULT::Cancelled;
}

MQL_RESULT
MQLock::cancel_writer_lock_no_pred(uint32_t me)
{
	return MQL_RESULT::Cancelled;
}

void
RWLock::r_lock()
{
	int expected, desired;
	expected = counter.load(std::memory_order_acquire);
	for (;;) {
		if (expected != -1) desired = expected + 1;
		else {
			expected = counter.load(std::memory_order_acquire);
			continue;
		}
		if (counter.compare_exchange_strong(expected, desired, std::memory_order_acq_rel, std::memory_order_acquire)) break;
	}
}

bool
RWLock::r_trylock()
{
	int expected, desired;
	expected = counter.load(std::memory_order_acquire);
	for (;;) {
		if (expected != -1) desired = expected + 1;
		else return false;

		if (counter.compare_exchange_strong(expected, desired, std::memory_order_acq_rel, std::memory_order_acquire)) return true;
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
	expected = counter.load(std::memory_order_acquire);
	for (;;) {
		if (expected != 0) {
			expected = counter.load(std::memory_order_acquire);
			continue;
		}
		if (counter.compare_exchange_strong(expected, desired, std::memory_order_acq_rel, std::memory_order_acquire)) break;
	}
}

bool
RWLock::w_trylock()
{
	int expected, desired(-1);
	expected = counter.load(std::memory_order_acquire);
	for (;;) {
		if (expected != 0) return false;
		if (counter.compare_exchange_strong(expected, desired, std::memory_order_acq_rel, std::memory_order_acquire)) return true;
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
