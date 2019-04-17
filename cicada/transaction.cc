
#include <string.h> // memcpy
#include <sys/time.h>
#include <stdio.h>
#include <xmmintrin.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "../include/debug.hpp"
#include "../include/tsc.hpp"

#include "include/transaction.hpp"
#include "include/common.hpp"
#include "include/timeStamp.hpp"
#include "include/version.hpp"

extern bool chkClkSpan(const uint64_t start, const uint64_t stop, const uint64_t threshold);
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
  stop = rdtscp();
  if (chkClkSpan(start, stop, 100 * CLOCKS_PER_US)) {
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
      return writeVal;
      // tanabe の実験では，スレッドごとに新しく書く値は決まっている．
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
  Version *version = Table[key].ldAcqLatest();
  if (version->ldAcqStatus() == VersionStatus::pending) {
    if (version->ldAcqWts() < trts)
      while (version->ldAcqStatus() == VersionStatus::pending);
  }
  while (version->ldAcqWts() > trts
      || version->ldAcqStatus() != VersionStatus::committed) {
    version = version->ldAcqNext();
  }

  // for fairness
  // ultimately, it is wasteful in prototype system.
  memcpy(returnVal, version->val, VAL_SIZE);

  //if read-only, not track or validate readSet
  if (this->ronly) {
    return version->val;
  }
  else {
    readSet.emplace_back(key, version);
    return version->val;
  }
}

void
TxExecutor::twrite(unsigned int key)
{
  //if n E writeSet
  for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
    if ((*itr).key == key) {
      return;
      // tanabe の実験では，スレッドごとに新たに書く値が決まっている．
    }
  }
    
  Version *version = Table[key].ldAcqLatest();

  //Search version (for early abort check)
  //search latest (committed or pending) version and use for early abort check
  //pending version may be committed, so use for early abort check.
  while (version->ldAcqStatus() == VersionStatus::aborted)
    version = version->ldAcqNext();

  //early abort check(EAC)
  //here, version->status is commit or pending.
  if ((version->ldAcqRts() > this->wts.ts) && (version->ldAcqStatus() == VersionStatus::committed)) {
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
  
  if (getInlineVersionRight(key)) {
    Table[key].inlineVersion.set(0, this->wts.ts);
    writeSet.emplace_back(key, &Table[key].inlineVersion);
  }
  else {
    Version *newObject;
    newObject = new Version(0, this->wts.ts);
    writeSet.emplace_back(key, newObject);
  }

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
      Version *version = Table[(*itr).key].ldAcqLatest();
      while (version->ldAcqStatus() != VersionStatus::committed)
        version = version->ldAcqNext();

      if (version->ldAcqRts() > this->wts.ts) return false;
    }
  }
  
  //Install pending version
  for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
    Version *expected, *version;
    for (;;) {
      version = expected = Table[(*itr).key].ldAcqLatest();

      if (version->ldAcqStatus() == VersionStatus::pending) {
        if (this->wts.ts < version->ldAcqWts()) return false;
        while (version->ldAcqStatus() == VersionStatus::pending);
      }

      while (version->ldAcqStatus() != VersionStatus::committed)
        version = version->ldAcqNext();
      //to avoid cascading aborts.
      //install pending version step blocks concurrent 
      //transactions that share the same visible version 
      //and have higher timestamp than (tx.ts).

      // to keep versions ordered by wts in the version list
      // if version is pending version, concurrent transaction that has lower timestamp is aborted.
      if (this->wts.ts < version->ldAcqWts()) return false;

      (*itr).newObject->status.store(VersionStatus::pending, memory_order_release);
      (*itr).newObject->strRelNext(Table[(*itr).key].ldAcqLatest());
      if (Table[(*itr).key].latest.compare_exchange_strong(expected, (*itr).newObject, memory_order_acq_rel, memory_order_acquire)) break;
    }

    (*itr).finishVersionInstall = true;
  }

  //Read timestamp update
  for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
    uint64_t expected;
    expected = (*itr).ver->ldAcqRts();
    for (;;) {
      if (expected > this->wts.ts) break;
      if ((*itr).ver->rts.compare_exchange_strong(expected, this->wts.ts, memory_order_acq_rel, memory_order_acquire)) break;
    }
  }

  // version consistency check
  //(a) every previously visible version v of the records in the read set is the currently visible version to the transaction.
  for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
    Version *version, *init;
    for (;;) {
      version  = init = Table[(*itr).key].ldAcqLatest();

      if (version->ldAcqStatus() == VersionStatus::pending
          && version->ldAcqWts() < this->wts.ts) {
        // pending version であり，観測範囲内であれば，
        // その termination を見届ける必要がある．
        // 最終的にコミットされたら，View が壊れる．
        // 元々それを読まなければいけなかったのでアボート
        while (version->ldAcqStatus() == VersionStatus::pending);
        if (version->ldAcqStatus() == VersionStatus::committed) return false;
      }

      while (version->ldAcqStatus() != VersionStatus::committed 
          || version->ldAcqWts() > this->wts.ts)
        version = version->ldAcqNext();

      if ((*itr).ver != version) return false;

      if (init == Table[(*itr).key].ldAcqLatest()) break;
      // else, the validation was interrupted.
    }
  }

  // (b) every currently visible version v of the records in the write set satisfies (v.rts) <= (tx.ts)
  for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
    Version *version = (*itr).newObject->ldAcqNext();
    while (version->ldAcqStatus() != VersionStatus::committed)
      version = version->ldAcqNext();
    if (version->ldAcqRts() > this->wts.ts) return false;
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

    double threshold = CLOCKS_PER_US * IO_TIME_NS / 1000;
    spinstart = rdtscp();
    while ((rdtscp() - spinstart) < threshold) {}  //spin-wait

    SwalLock.w_unlock();
  }
  else {  //group commit
    SwalLock.w_lock();
    for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
      SLogSet[GROUP_COMMIT_INDEX[0].obj] = (*itr).newObject;
      ++GROUP_COMMIT_INDEX[0].obj;
    }

    if (GROUP_COMMIT_COUNTER[0].obj == 0) {
      grpcmt_start = rdtscp(); // it can also initialize.
    }

    ++GROUP_COMMIT_COUNTER[0].obj;

    if (GROUP_COMMIT_COUNTER[0].obj == GROUP_COMMIT) {
      double threshold = CLOCKS_PER_US * IO_TIME_NS / 1000;
      spinstart = rdtscp();
      while ((rdtscp() - spinstart) < threshold) {}  //spin-wait

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
    double threshold = CLOCKS_PER_US * IO_TIME_NS / 1000;
    spinstart = rdtscp();
    while ((rdtscp() - spinstart) < threshold) {}  //spin-wait
  } 
  else {
    for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
      PLogSet[thid][GROUP_COMMIT_INDEX[thid].obj] = (*itr).newObject;
      ++GROUP_COMMIT_INDEX[this->thid].obj;
    }

    if (GROUP_COMMIT_COUNTER[this->thid].obj == 0) {
      grpcmt_start = rdtscp(); // it can also initialize.
      ThreadRtsArrayForGroup[this->thid].obj = this->rts;
    }

    ++GROUP_COMMIT_COUNTER[this->thid].obj;

    if (GROUP_COMMIT_COUNTER[this->thid].obj == GROUP_COMMIT) {
      // it gives latency instead of flush.
      double threshold = CLOCKS_PER_US * IO_TIME_NS / 1000;
      spinstart = rdtscp();
      while ((rdtscp() - spinstart) < threshold) {}  //spin-wait 

      gcpv();
    } 
  }
}

inline void
TxExecutor::cpv()  //commit pending versions
{
  for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
    gcq.push_back(GCElement((*itr).key, (*itr).newObject, (*itr).newObject->wts));
    memcpy((*itr).newObject->val, writeVal, VAL_SIZE);
    (*itr).newObject->status.store(VersionStatus::committed, std::memory_order_release);
  }

  //std::cout << gcq.size() << std::endl;
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
TxExecutor::wSetClean()
{
  for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
    if ((*itr).finishVersionInstall)
      (*itr).newObject->status.store(VersionStatus::aborted, std::memory_order_release);
    else
      if ((*itr).newObject != &Table[(*itr).key].inlineVersion) delete (*itr).newObject;
      else returnInlineVersionRight((*itr).key);
  }

  writeSet.clear();
}

void 
TxExecutor::earlyAbort()
{
  wSetClean();
  readSet.clear();

  if (GROUP_COMMIT) {
    chkGcpvTimeout();
  }

  this->wts.set_clockBoost(CLOCKS_PER_US);
  continuingCommit = 0;
  this->status = TransactionStatus::abort;
}

void
TxExecutor::abort()
{
  wSetClean();
  readSet.clear();

  //pending versionのステータスをabortedに変更
  for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
    (*itr).newObject->status.store(VersionStatus::aborted, std::memory_order_release);
  }

  if (GROUP_COMMIT) {
    chkGcpvTimeout();
  }


  this->wts.set_clockBoost(CLOCKS_PER_US);
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
    grpcmt_stop = rdtscp();
    if (chkClkSpan(grpcmt_start, grpcmt_stop, GROUP_COMMIT_TIMEOUT_US * CLOCKS_PER_US)) {
      gcpv();
      return true;
    }
  }
  else if (S_WAL) {
    grpcmt_stop = rdtscp();
    if (chkClkSpan(grpcmt_start, grpcmt_stop, GROUP_COMMIT_TIMEOUT_US * CLOCKS_PER_US)) {
      SwalLock.w_lock();
      gcpv();
      SwalLock.w_unlock();
      return true;
    }
  }

  return false;
}

void 
TxExecutor::mainte(CicadaResult &res)
{
  //Maintenance
  //Schedule garbage collection
  //Declare quiescent state
  //Collect garbage created by prior transactions
  
  //バージョンリストにおいてMinRtsよりも古いバージョンの中でコミット済み最新のもの
  //以外のバージョンは全てデリート可能。絶対に到達されないことが保証される.
  //

  if (__atomic_load_n(&(GCExecuteFlag[thid].obj), __ATOMIC_ACQUIRE) == 1) {
    ++res.localGCCounts;
    while (!gcq.empty()) {
      if (gcq.front().wts >= MinRts.load(memory_order_acquire)) break;
    
      // (a) acquiring the garbage collection lock succeeds
      if (getGCRight(gcq.front().key) == false) {
        // fail acquiring the lock
        gcq.pop_front();
        continue;
      }

      // (b) v.wts > record.min_wts
      if (gcq.front().wts <= Table[gcq.front().key].min_wts) {
        // releases the lock
        Table[gcq.front().key].gClock.store(0, memory_order_release);
        gcq.pop_front();
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
        if (delTarget != &Table[gcq.front().key].inlineVersion) delete delTarget;
        else returnInlineVersionRight(gcq.front().key);
        ++res.localGCVersions;
        delTarget = tmp;
      }
      
      // releases the lock
      returnGCRight(gcq.front().key);
      gcq.pop_front();
    }

    __atomic_store_n(&(GCExecuteFlag[thid].obj), 0, __ATOMIC_RELEASE);
  }

  this->GCstop = rdtscp();
  if (chkClkSpan(this->GCstart, this->GCstop, GC_INTER_US * CLOCKS_PER_US)) {
    __atomic_store_n(&(GCFlag[thid].obj),  1, __ATOMIC_RELEASE);
    this->GCstart = this->GCstop;
  }
}

void
TxExecutor::writePhase()
{
  if (GROUP_COMMIT) {
    //check time out of commit pending versions
    chkGcpvTimeout();
  } else {
    cpv();
  }

  //log write set & possibly group commit pending version
  if (P_WAL) pwal();
  if (S_WAL) swal();

  //ここまで来たらコミットしている
  this->wts.set_clockBoost(0);
  readSet.clear();
  writeSet.clear();
}

