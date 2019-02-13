
#include <atomic>
#include <algorithm>
#include <bitset>

#include "../include/debug.hpp"

#include "include/common.hpp"
#include "include/transaction.hpp"
#include "include/version.hpp"

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

  while (txid < ver->cstamp.load(memory_order_acquire)) {
    //printf("txid %d, (verCstamp >> 1) %d\n", txid, verCstamp >> 1);
    //fflush(stdout);
    ver = ver->committed_prev;
    if (ver == nullptr) {
      ERR;
    }
  }

  readSet.emplace_back(key, ver);
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
  desired->cstamp.store(this->txid, memory_order_release);  // storing before CAS because it will be accessed from read operation, write operation and garbage collection.
  desired->status.store(VersionStatus::inFlight, memory_order_release);

  Version *vertmp;
  expected = Table[key].latest.load(std::memory_order_acquire);
  for (;;) {
    // w-w conflict with concurrent transactions.
    if (expected->status.load(memory_order_acquire) == VersionStatus::inFlight) {

      uint64_t rivaltid = expected->cstamp.load(memory_order_acquire);
      if (this->txid <= rivaltid) {
      //if (1) { // no-wait で abort させても性能劣化はほぼ起きていない．
      //性能が向上されるケースもある．
        this->status = TransactionStatus::aborted;
        delete desired;
        return;
      }

      expected = Table[key].latest.load(std::memory_order_acquire);
      continue;
    }
    
    // if a head version isn't committed version
    vertmp = expected;
    while (vertmp->status.load(memory_order_acquire) != VersionStatus::committed) {
      vertmp = vertmp->committed_prev;
      if (vertmp == nullptr) ERR;
    }

    // vertmp is committed latest version.
    if (txid < vertmp->cstamp.load(memory_order_acquire)) {  
      //  write - write conflict, first-updater-wins rule.
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

  writeSet.emplace_back(key, desired);
}

void
Transaction::commit()
{
  this->cstamp = ++Lsn;
  status = TransactionStatus::committed;

  for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
    (*itr).ver->cstamp.store(this->cstamp, memory_order_release);
    (*itr).ver->status.store(VersionStatus::committed, memory_order_release);
    gcobject.gcqForVersion.push(GCElement((*itr).key, (*itr).ver, cstamp));
  }

  //logging

  readSet.clear();
  writeSet.clear();

  ++rsobject.localCommitCounts;
  return;
}

void
Transaction::abort()
{
  status = TransactionStatus::aborted;

  for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
    (*itr).ver->status.store(VersionStatus::aborted, memory_order_release);
    gcobject.gcqForVersion.push(GCElement((*itr).key, (*itr).ver, this->txid));
  }

  readSet.clear();
  writeSet.clear();

  ++rsobject.localAbortCounts;
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

