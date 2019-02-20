
#include <string.h> // memcpy
#include <sys/time.h>
#include <stdio.h>
#include <xmmintrin.h>

#include <algorithm>
#include <fstream>
#include <string>
#include <vector>

#include "../include/debug.hpp"
#include "../include/tsc.hpp"

#include "include/transaction.hpp"
#include "include/common.hpp"
#include "include/timeStamp.hpp"
#include "include/version.hpp"

extern bool chkSpan(struct timeval &start, struct timeval &stop, long threshold);
extern bool chkClkSpan(uint64_t &start, uint64_t &stop, uint64_t threshold);
extern void displaySLogSet();
extern void displayDB();

using namespace std;

#define MSK_TID 0b11111111

void TxExecutor::tbegin(bool ronly)
{
  if (ronly) this->ronly = true;
  else this->ronly = false;

  this->status = TransactionStatus::inflight;
  this->wts.generateTimeStamp(thid);
  __atomic_store_n(&(ThreadWtsArray[thid].obj), this->wts.ts, __ATOMIC_RELEASE);
  this->rts = MinWts.load(std::memory_order_acquire) - 1;
  __atomic_store_n(&(ThreadRtsArray[thid].obj), this->rts, __ATOMIC_RELEASE);

  // one-sided synchronization
  // tanabe... disabled.
  // When the database size is small that all record can be on
  // cache, remote worker's clock can also be on cache after one-sided
  // synchronization.
  // After that, by transaction processing, cache line invalidation
  // often occurs and degrade throughput.
  /* 
  stop = rdtsc();
  if (chkClkSpan(start, stop, 100 * CLOCK_PER_US)) {
    uint64_t maxwts;
      maxwts = __atomic_load_n(&(ThreadWtsArray[1].obj), __ATOMIC_ACQUIRE);
    //record the fastest one, and adjust it.
    //one-sided synchronization
    uint64_t check;
    for (unsigned int i = 2; i < THREAD_NUM; ++i) {
      check = __atomic_load_n(&(ThreadWtsArray[i].obj), __ATOMIC_ACQUIRE);
      if (maxwts < check) maxwts = check;
    }
    maxwts = maxwts & (~MSK_TID);
    maxwts = maxwts | thid;
    this->wts.ts = maxwts;
    __atomic_store_n(&(ThreadWtsArray[thid].obj), this->wts.ts, __ATOMIC_RELEASE);

    //modify the start position of stopwatch.
    start = stop;
  }
  */
}

char *
TxExecutor::tread(unsigned int key)
{
  //read-own-writes
  //if n E write set
  for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
    if ((*itr).key == key) {
      return (*itr).newObject->val;
    }
  }
  // if n E read set
  for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
    if ((*itr).key == key) {
      return (*itr).ver->val;
    }
  }

  //else
  uint64_t trts;
  if (this->ronly) {
    trts = this->rts;
  }
  else  trts = this->wts.ts;
  //-----

  //Search version
  Version *version = Table[key].latest;
  while (version->wts.load(memory_order_acquire) > trts) {
    version = version->next.load(std::memory_order_acquire);
    //if version is not found.
    if (version == nullptr) {
      ERR;
    } 
  }
  
  //timestamp condition is ok. and check status
  VersionStatus loadst = version->status.load(memory_order_acquire);
  while (loadst != VersionStatus::committed && loadst != VersionStatus::precommitted) {
    if (loadst != VersionStatus::aborted) {
      if (NLR) {
        //spin wait
        if (GROUP_COMMIT) {
          spinstart = rdtsc();
          while (loadst == VersionStatus::pending){
            spinstop = rdtsc();
            if (chkClkSpan(spinstart, spinstop, SPIN_WAIT_TIMEOUT_US * CLOCK_PER_US) ) {
              earlyAbort();
              return nullptr;
            }
            loadst = version->status.load(memory_order_acquire);
          }
        } else {
          while (loadst == VersionStatus::pending) {
            loadst = version->status.load(memory_order_acquire);
          }
        }
        if (loadst == VersionStatus::committed) break;
      } else if (ELR) {
        while (loadst == VersionStatus::pending) {
          loadst = version->status.load(memory_order_acquire);    
        }
        if (loadst == VersionStatus::committed || loadst == VersionStatus::precommitted) break;
      }
    }

    version = version->next.load(memory_order_acquire);
    loadst = version->status.load(memory_order_acquire);
    //if version is not found.
    if (version == nullptr) {
      ERR;
    }
  }

  //hit version
  //if read-only, not track or validate readSet
  if (this->ronly) {
    return version->val;
  }

  readSet.push_back(ReadElement(key, version));
  return version->val;
}

void
TxExecutor::twrite(unsigned int key)
{
  //if n E writeSet
  for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
    if ((*itr).key == key) {
      memcpy((*itr).newObject->val, writeVal, VAL_SIZE);
      return;
    }
  }
    
  Version *version = Table[key].latest.load();

  //Search version (for early abort check)
  //search latest (committed or pending) version and use for early abort check
  //pending version may be committed, so use for early abort check.
  VersionStatus loadst = version->status.load(memory_order_acquire);
  // it uses loadst for reducing a number of atomic-load.
  while (loadst == VersionStatus::aborted) {
    version = version->next.load(std::memory_order_acquire);
    loadst = version->status.load(memory_order_acquire);

    //if version is not found.
    if (version == nullptr) ERR;
  }
  //hit version

  //early abort check(EAC)
  //here, version->status is commit or pending.
  if ((version->rts.load(memory_order_acquire) > this->wts.ts) && (loadst == VersionStatus::committed)) {
    //it must be aborted in validation phase, so early abort.
    this->status = TransactionStatus::abort;
    return; 
  }
  else if (version->wts.load(memory_order_acquire) > this->wts.ts) {
    //if pending, it don't have to be aborted.
    //But it may be aborted, so early abort.
    //if committed, it must be aborted in validaiton phase due to order of newest to oldest.
    this->status = TransactionStatus::abort;
    return; 
  }

  if (NLR) {
    while (loadst != VersionStatus::committed) {
      while (loadst == VersionStatus::pending) {
        //spin-wait
        //if nlr & group commit, it happens dead lock.
        //dead lock prevention is time out.
        if (GROUP_COMMIT) {
          spinstart = rdtsc();
          while (loadst == VersionStatus::pending){
            spinstop = rdtsc();
            if (chkClkSpan(spinstart, spinstop, SPIN_WAIT_TIMEOUT_US * CLOCK_PER_US) ) {
              earlyAbort();
              return;
            }
            loadst = version->status.load(memory_order_acquire);
          }
        } 
        loadst = version->status.load(memory_order_acquire);
      }
      if (loadst == VersionStatus::committed) break;
    
      //status == aborted
      version = version->next.load(std::memory_order_acquire);
      loadst = version->status.load(memory_order_acquire);
      if (version == nullptr) {
        ERR;
      }
    }
  } 
  else if (ELR) {
    //here, the status is pending or committed or precomitted.
    while (loadst == VersionStatus::pending) 
      loadst = version->status.load(std::memory_order_acquire);

    while (loadst != VersionStatus::precommitted && loadst != VersionStatus::committed) {
      //status == aborted
      version = version->next.load(std::memory_order_acquire);
      loadst = version->status.load(memory_order_acquire);
      if (version == nullptr) ERR;
    }
  }

  Version *newObject;
  newObject = new Version(0, this->wts.ts, writeVal);
  writeSet.push_back(WriteElement(key, version, newObject));
  return;
}

bool
TxExecutor::validation() 
{
  if (continuingCommit < 5) {
    // Two optimizations can add unnecessary overhead under low contention 
    // because they do not improve the performance of uncontended workloads.
    // Each thread adaptively omits both steps if the recent transactions have been committed (5 in a row in our implementation).
    //
    // Sort write set by contention
    partial_sort(writeSet.begin(), writeSet.begin() + (writeSet.size() / 2), writeSet.end());

    // Pre-check version consistency
    // (b) every currently visible version v of the records in the write set satisfies (v.rts) <= (tx.ts)
    for (auto itr = writeSet.begin(); itr != writeSet.begin() + (writeSet.size() / 2); ++itr) {
        if ((*itr).sourceObject->rts.load(memory_order_acquire) > this->wts.ts) return false;
    }
  }
  
  //Install pending version
  for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
    
    Version *expected;
    expected = Table[(*itr).key].latest.load();
    for (;;) {
      Version *version = expected;

      VersionStatus loadst = version->status.load(memory_order_acquire);
      while (loadst != VersionStatus::committed && loadst != VersionStatus::precommitted) {

        while (loadst == VersionStatus::aborted) {
          version = version->next.load(std::memory_order_acquire);
          loadst = version->status.load(memory_order_acquire);
        }

        //to avoid cascading aborts.
        //install pending version step blocks concurrent transactions that share the same visible version and have higher timestamp than (tx.ts).
        if (NLR && GROUP_COMMIT) {
          spinstart = rdtsc();
          while (loadst == VersionStatus::pending) {
            spinstop = rdtsc();
            if (chkClkSpan(spinstart, spinstop, SPIN_WAIT_TIMEOUT_US * CLOCK_PER_US) ) {
              return false;
            }
            if (this->wts.ts < version->wts.load(memory_order_acquire)) return false;
            loadst = version->status.load(memory_order_acquire);
          }
        } else {
          while (loadst == VersionStatus::pending) {
            if (this->wts.ts < version->wts.load(memory_order_acquire)) return false;
            loadst = version->status.load(memory_order_acquire);
          }
        //now, version->status is not pending.
        }
      }  

      // to keep versions ordered by wts in the version list
      // if version is pending version, concurrent transaction that has lower timestamp is aborted.
      // OR
      // validation consistency check (pre-check)
      // (b) every currently visible version v of the records in the write set satisfies (v.rts) <= (tx.ts)
      if (this->wts.ts < version->wts.load(memory_order_acquire) || this->wts.ts < version->rts.load(memory_order_acquire)) return false;

      (*itr).newObject->status.store(VersionStatus::pending, memory_order_release);
      (*itr).newObject->next.store(Table[(*itr).key].latest.load(), memory_order_release);
      if (Table[(*itr).key].latest.compare_exchange_strong(expected, (*itr).newObject, memory_order_acq_rel, memory_order_acquire)) break;
    }
  }

  //Read timestamp update
  for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
    uint64_t expected;
    expected = (*itr).ver->rts.load(memory_order_acquire);
    for (;;) {
      if (expected > this->wts.ts) break;
      if ((*itr).ver->rts.compare_exchange_strong(expected, this->wts.ts, memory_order_acq_rel, memory_order_acquire)) break;
    }
  }

  // version consistency check
  //(a) every previously visible version v of the records in the read set is the currently visible version to the transaction.
  for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
    if (ELR) {
      if ((*itr).ver->status.load(std::memory_order_acquire) != VersionStatus::committed && (*itr).ver->status.load(std::memory_order_acquire) != VersionStatus::precommitted) return false;
    } 
    else 
      if ((*itr).ver->status.load(std::memory_order_acquire) != VersionStatus::committed) return false;
  }
  // (b) every currently visible version v of the records in the write set satisfies (v.rts) <= (tx.ts)
  for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
      if ((*itr).sourceObject->rts.load(memory_order_acquire) > this->wts.ts) return false;
  }

  return true;
}

void
TxExecutor::swal()
{
  if (!GROUP_COMMIT) {  //non-group commit
    SwalLock.w_lock();

    int i = 0;
    for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
      SLogSet[i] = (*itr).newObject;
      ++i;
    }

    double threshold = CLOCK_PER_US * IO_TIME_NS / 1000;
    spinstart = rdtsc();
    while ((rdtsc() - spinstart) < threshold) {}  //spin-wait

    SwalLock.w_unlock();
  }
  else {  //group commit
    SwalLock.w_lock();
    for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
      SLogSet[GROUP_COMMIT_INDEX[0].obj] = (*itr).newObject;
      ++GROUP_COMMIT_INDEX[0].obj;
    }

    if (GROUP_COMMIT_COUNTER[0].obj == 0) {
      grpcmt_start = rdtsc(); // it can also initialize.
    }

    ++GROUP_COMMIT_COUNTER[0].obj;

    if (GROUP_COMMIT_COUNTER[0].obj == GROUP_COMMIT) {
      double threshold = CLOCK_PER_US * IO_TIME_NS / 1000;
      spinstart = rdtsc();
      while ((rdtsc() - spinstart) < threshold) {}  //spin-wait

      //group commit pending version.
      gcpv();
    }
    SwalLock.w_unlock();
  }
}

void
TxExecutor::pwal()
{
  if (!GROUP_COMMIT) {
    int i = 0;
    for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
      PLogSet[thid][i] = (*itr).newObject;
      ++i;
    }

    // it gives lat ency instead of flush.
    double threshold = CLOCK_PER_US * IO_TIME_NS / 1000;
    spinstart = rdtsc();
    while ((rdtsc() - spinstart) < threshold) {}  //spin-wait
  } 
  else {
    for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
      PLogSet[thid][GROUP_COMMIT_INDEX[thid].obj] = (*itr).newObject;
      ++GROUP_COMMIT_INDEX[this->thid].obj;
    }

    if (GROUP_COMMIT_COUNTER[this->thid].obj == 0) {
      grpcmt_start = rdtsc(); // it can also initialize.
      ThreadRtsArrayForGroup[this->thid].obj = this->rts;
    }

    ++GROUP_COMMIT_COUNTER[this->thid].obj;

    if (GROUP_COMMIT_COUNTER[this->thid].obj == GROUP_COMMIT) {
      // it gives latency instead of flush.
      double threshold = CLOCK_PER_US * IO_TIME_NS / 1000;
      spinstart = rdtsc();
      while ((rdtsc() - spinstart) < threshold) {}  //spin-wait 

      gcpv();
    } 
  }
}

inline void
TxExecutor::cpv()  //commit pending versions
{
  for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
    (*itr).newObject->status.store(VersionStatus::committed, std::memory_order_release);
  }

}

void
TxExecutor::precpv() 
{
  for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
    (*itr).newObject->status.store(VersionStatus::precommitted, std::memory_order_release);
  }

}
  
void 
TxExecutor::gcpv()
{
  if (S_WAL) {
    for (unsigned int i = 0; i < GROUP_COMMIT_INDEX[0].obj; ++i) {
      SLogSet[i]->status.store(VersionStatus::committed, memory_order_release);
    }
    GROUP_COMMIT_COUNTER[0].obj = 0;
    GROUP_COMMIT_INDEX[0].obj = 0;
  }
  else if (P_WAL) {
    for (unsigned int i = 0; i < GROUP_COMMIT_INDEX[thid].obj; ++i) {
      PLogSet[thid][i]->status.store(VersionStatus::committed, memory_order_release);
    }
    GROUP_COMMIT_COUNTER[thid].obj = 0;
    GROUP_COMMIT_INDEX[thid].obj = 0;
  }
}

void 
TxExecutor::earlyAbort()
{
  writeSet.clear();
  readSet.clear();

  if (GROUP_COMMIT) {
    chkGcpvTimeout();
  }

  this->wts.set_clockBoost(CLOCK_PER_US);
  ++rsobject.localAbortCounts;
  continuingCommit = 0;
  this->status = TransactionStatus::abort;
}

void
TxExecutor::abort()
{
  //pending versionのステータスをabortedに変更
  for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
    (*itr).newObject->status.store(VersionStatus::aborted, std::memory_order_release);
  }

  if (GROUP_COMMIT) {
    chkGcpvTimeout();
  }

  readSet.clear();
  writeSet.clear();

  this->wts.set_clockBoost(CLOCK_PER_US);
  ++rsobject.localAbortCounts;
  continuingCommit = 0;
}

void
TxExecutor::displayWset()
{
  for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
    printf("%d ", (*itr).key);
  }
  cout << endl;
}

bool
TxExecutor::chkGcpvTimeout()
{
  if (P_WAL) {
    grpcmt_stop = rdtsc();
    if (chkClkSpan(grpcmt_start, grpcmt_stop, GROUP_COMMIT_TIMEOUT_US * CLOCK_PER_US)) {
      gcpv();
      return true;
    }
  }
  else if (S_WAL) {
    grpcmt_stop = rdtsc();
    if (chkClkSpan(grpcmt_start, grpcmt_stop, GROUP_COMMIT_TIMEOUT_US * CLOCK_PER_US)) {
      SwalLock.w_lock();
      gcpv();
      SwalLock.w_unlock();
      return true;
    }
  }

  return false;
}

void 
TxExecutor::mainte()
{
  //Maintenance
  //Schedule garbage collection
  //Declare quiescent state
  //Collect garbage created by prior transactions
  
  //バージョンリストにおいてMinRtsよりも古いバージョンの中でコミット済み最新のもの
  //以外のバージョンは全てデリート可能。絶対に到達されないことが保証される.
  //

  if (__atomic_load_n(&(GCExecuteFlag[thid].obj), __ATOMIC_ACQUIRE) == 1) {
    ++rsobject.localGCCounts;
    while (!gcq.empty()) {
      if (gcq.front().wts >= MinRts.load(memory_order_acquire)) break;
    
      // (a) acquiring the garbage collection lock succeeds
      uint8_t zero = 0;
      if (!Table[gcq.front().key].gClock.compare_exchange_weak(zero, this->thid, std::memory_order_acq_rel, std::memory_order_acquire)) {
        // fail acquiring the lock
        gcq.pop();
        continue;
      }

      // (b) v.wts > record.min_wts
      if (gcq.front().wts <= Table[gcq.front().key].min_wts) {
        // releases the lock
        Table[gcq.front().key].gClock.store(0, memory_order_release);
        gcq.pop();
        continue;
      }
      //this pointer may be dangling.

      Version *delTarget = gcq.front().ver->next.load(std::memory_order_acquire);

      // the thread detaches the rest of the version list from v
      gcq.front().ver->next.store(nullptr, std::memory_order_release);
      // updates record.min_wts
      Table[gcq.front().key].min_wts.store(gcq.front().ver->wts, memory_order_release);

      while (delTarget != nullptr) {
        //nextポインタ退避
        Version *tmp = delTarget->next.load(std::memory_order_acquire);
        delete delTarget;
        delTarget = tmp;
      }
      
      // releases the lock
      Table[gcq.front().key].gClock.store(0, memory_order_release);
      gcq.pop();
    }

    __atomic_store_n(&(GCExecuteFlag[thid].obj), 0, __ATOMIC_RELEASE);
  }

  this->GCstop = rdtsc();
  if (chkClkSpan(this->GCstart, this->GCstop, GC_INTER_US * CLOCK_PER_US)) {
    __atomic_store_n(&(GCFlag[thid].obj),  1, __ATOMIC_RELEASE);
    this->GCstart = this->GCstop;
  }
}

void
TxExecutor::writePhase()
{
  //early lock release
  //ログを書く前に保有するバージョンステータスをprecommittedにする．
  if (ELR)  precpv();

  //write phase
  //log write set & possibly group commit pending version
  if (P_WAL) pwal();
  if (S_WAL) swal();

  if (!GROUP_COMMIT) {
    cpv();
  } else {
    //check time out of commit pending versions
    chkGcpvTimeout();
  }

  for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
    gcq.push(GCElement((*itr).key, (*itr).newObject, (*itr).newObject->wts));
  }

  //ここまで来たらコミットしている
  this->wts.set_clockBoost(0);
  readSet.clear();
  writeSet.clear();

  ++continuingCommit;
  ++rsobject.localCommitCounts;
}

