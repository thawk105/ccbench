#include "include/common.hpp"
#include "include/debug.hpp"
#include "include/transaction.hpp"
#include "include/version.hpp"
#include <atomic>
#include <algorithm>
#include <bitset>

using namespace std;

inline
SetElement *
Transaction::searchReadSet(unsigned int key) 
{
	for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
		if ((*itr).key == key) return &(*itr);
	}

	return nullptr;
}

inline
SetElement *
Transaction::searchWriteSet(unsigned int key) 
{
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		if ((*itr).key == key) return &(*itr);
	}

	return nullptr;
}

void
Transaction::tbegin()
{
  TransactionTable *newElement, *tmt;

  tmt = __atomic_load_n(&TMT[thid], __ATOMIC_ACQUIRE);
	if (this->status == TransactionStatus::committed) {
    this->txid = cstamp;
    newElement = new TransactionTable(0, cstamp);
  }
  else {
    this->txid = TMT[thid]->lastcstamp.load(memory_order_acquire);
    newElement = new TransactionTable(0, tmt->lastcstamp.load(std::memory_order_acquire));
  }

	for (unsigned int i = 1; i < THREAD_NUM; ++i) {
    if (thid == i) continue;
    do {
      tmt = __atomic_load_n(&TMT[i], __ATOMIC_ACQUIRE);
    } while (tmt == nullptr);
		this->txid = max(this->txid, tmt->lastcstamp.load(memory_order_acquire));
	}
	this->txid += 1;
  newElement->txid = this->txid;
	
  TransactionTable *expected, *desired;
  tmt = __atomic_load_n(&TMT[thid], __ATOMIC_ACQUIRE);
  expected = tmt;
  gcobject.gcqForTMT.push(expected);
  for (;;) {
    desired = newElement;
    if (__atomic_compare_exchange_n(&TMT[thid], &expected, desired, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) break;
  }

	status = TransactionStatus::inFlight;
}

int
Transaction::tread(unsigned int key)
{
	//if it already access the key object once.
	// w
	SetElement *inW = searchWriteSet(key);
	if (inW) {
		return inW->ver->val;
	}

	SetElement *inR = searchReadSet(key);
	if (inR) {
		return inR->ver->val;
	}

	// if v not in t.writes:
	Version *ver = Table[key].latest.load(std::memory_order_acquire);
	if (ver->status.load(memory_order_acquire) != VersionStatus::committed) {
		ver = ver->committed_prev;
	}

	uint64_t verCstamp = ver->cstamp.load(memory_order_acquire);
	while (txid < (verCstamp >> 1)) {
		//printf("txid %d, (verCstamp >> 1) %d\n", txid, verCstamp >> 1);
		//fflush(stdout);
		ver = ver->committed_prev;
		if (ver == nullptr) {
			cout << "txid : " << txid
				<< ", verCstamp : " << verCstamp << endl;
			ERR;
		}
		verCstamp = ver->cstamp.load(memory_order_acquire);
	}

	readSet.push_back(SetElement(key, ver));
	return ver->val;
}

void
Transaction::twrite(unsigned int key, unsigned int val)
{
	// if it already wrote the key object once.
	SetElement *inW = searchWriteSet(key);
	if (inW) {
		inW->ver->val = val;
		return;
	}

	// if v not in t.writes:
	//
	//first-updater-wins rule
	//Forbid a transaction to update  a record that has a committed head version later than its begin timestamp.
	
	Version *expected, *desired;
	desired = new Version();
	uint64_t tmptid = this->thid;
	tmptid = tmptid << 1;
	tmptid |= 1;
	desired->cstamp.store(tmptid, memory_order_release);	// storing before CAS because it will be accessed from read operation, write operation and garbage collection.

	Version *vertmp;
	expected = Table[key].latest.load(std::memory_order_acquire);
	for (;;) {
		// w-w conflict
		// first updater wins rule
		if (expected->status.load(memory_order_acquire) == VersionStatus::inFlight) {
			this->status = TransactionStatus::aborted;
			delete desired;
			return;
		}
		
    // if a head version isn't committed version
		vertmp = expected;
		while (vertmp->status.load(memory_order_acquire) != VersionStatus::committed) {
			vertmp = vertmp->committed_prev;
			if (vertmp == nullptr) ERR;
		}

    // vertmp is committed latest version.
		uint64_t verCstamp = vertmp->cstamp.load(memory_order_acquire);
		if (txid < (verCstamp >> 1)) {	
			//	write - write conflict, first-updater-wins rule.
			// Writers must abort if they would overwirte a version created after their snapshot.
			this->status = TransactionStatus::aborted;
			delete desired;
			return;
		}

		desired->prev = expected;
		desired->committed_prev = vertmp;
		if (Table[key].latest.compare_exchange_strong(expected, desired, memory_order_acq_rel, memory_order_acquire)) break;
	}
	desired->val = val;

	//ver->committed_prev->sstamp に TID を入れる
	uint64_t tmpTID = thid;
	tmpTID = tmpTID << 1;
	tmpTID |= 1;

	writeSet.push_back(SetElement(key, desired));
}

void
Transaction::commit()
{
	//this->cstamp = ++Lsn;
  this->cstamp = 2;
	status = TransactionStatus::committed;

	uint64_t verCstamp = cstamp;
	verCstamp = verCstamp << 1;
	verCstamp &= ~(1);

	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		(*itr).ver->cstamp.store(verCstamp, memory_order_release);
		(*itr).ver->status.store(VersionStatus::committed, memory_order_release);
	  gcobject.gcqForVersion.push(GCElement((*itr).key, (*itr).ver, verCstamp));
	}

	//logging

	readSet.clear();
	writeSet.clear();
	return;
}

void
Transaction::abort()
{
  status = TransactionStatus::aborted;

	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		(*itr).ver->status.store(VersionStatus::aborted, memory_order_release);
	  gcobject.gcqForVersion.push(GCElement((*itr).key, (*itr).ver, this->txid << 1));
	}

	readSet.clear();
	writeSet.clear();
}

void
Transaction::dispWS()
{
	cout << "th " << this->thid << " : write set : ";
	for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
		cout << "(" << (*itr).key << ", " << (*itr).ver->val << "), ";
	}
	cout << endl;
}

void
Transaction::dispRS()
{
	cout << "th " << this->thid << " : read set : ";
	for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
		cout << "(" << (*itr).key << ", " << (*itr).ver->val << "), ";
	}
	cout << endl;
}

