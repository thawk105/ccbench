
#include <stdint.h>
#include <stdlib.h>
#include <sys/syscall.h> // syscall(SYS_gettid),
#include <sys/types.h> // syscall(SYS_gettid), 
#include <unistd.h> // syscall(SYS_gettid), 

#include <atomic>
#include <bitset>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>

#include "../include/debug.hpp"
#include "../include/random.hpp"
#include "../include/tsc.hpp"
#include "../include/zipf.hpp"

#include "include/common.hpp"
#include "include/procedure.hpp"
#include "include/result.hpp"
#include "include/tuple.hpp"

bool chkClkSpan(uint64_t &start, uint64_t &stop, uint64_t threshold)
{
  uint64_t diff = 0;
  diff = stop - start;
  if (diff > threshold) return true;
  else return false;
}

void
displayDB()
{
  Tuple *tuple;
  Version *version;

  for (unsigned int i = 0; i < TUPLE_NUM; ++i) {
    tuple = &Table[i % TUPLE_NUM];
    cout << "------------------------------" << endl; // - 30
    cout << "key: " << tuple->key << endl;

    version = tuple->latest.load(std::memory_order_acquire);
    while (version != NULL) {
      cout << "val: " << version->val << endl;

      switch (version->status) {
        case VersionStatus::inFlight:
          cout << "status:  inFlight";
          break;
        case VersionStatus::aborted:
          cout << "status:  aborted";
          break;
        case VersionStatus::committed:
          cout << "status:  committed";
          break;
      }
      cout << endl;

      cout << "cstamp:  " << version->cstamp << endl;
      cout << "pstamp:  " << version->psstamp.pstamp << endl;
      cout << "sstamp:  " << version->psstamp.sstamp << endl;
      cout << endl;

      version = version->prev;
    }
    cout << endl;
  }
}

void
displayPRO(Procedure *pro)
{
  for (unsigned int i = 0; i < MAX_OPE; ++i) {
    cout << "(ope, key, val) = (";
    switch (pro[i].ope) {
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
  Version *verTmp;
  Xoroshiro128Plus rnd;
  rnd.init();

  try {
    if (posix_memalign((void**)&Table, 64, (TUPLE_NUM) * sizeof(Tuple)) != 0) ERR;
    for (unsigned int i = 0; i < TUPLE_NUM; ++i) {
      if (posix_memalign((void**)&Table[i].latest, 64, sizeof(Version)) != 0) ERR;
    }
  } catch (bad_alloc) {
    ERR;
  }

  for (unsigned int i = 0; i < TUPLE_NUM; ++i) {
    tmp = &Table[i];
    tmp->min_cstamp = 0;
    tmp->key = i;
    verTmp = tmp->latest.load(std::memory_order_acquire);
    verTmp->cstamp = 0;
    //verTmp->pstamp = 0;
    //verTmp->sstamp = UINT64_MAX & ~(1);
    verTmp->psstamp.pstamp = 0;
    verTmp->psstamp.sstamp = UINT32_MAX & ~(1);
    // cstamp, sstamp の最下位ビットは TID フラグ
    // 1の時はTID, 0の時はstamp
    verTmp->val = rnd.next() % TUPLE_NUM;
    verTmp->prev = nullptr;
    verTmp->committed_prev = nullptr;
    verTmp->status.store(VersionStatus::committed, std::memory_order_release);
    verTmp->readers.store(0, std::memory_order_release);
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
makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd, FastZipf &zipf)
{
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
naiveGarbageCollection() 
{
  TransactionTable *tmt;
  Result rsobject;

  uint32_t mintxID = UINT32_MAX;
  for (unsigned int i = 1; i < THREAD_NUM; ++i) {
    tmt = __atomic_load_n(&TMT[i], __ATOMIC_ACQUIRE);
    mintxID = min(mintxID, tmt->txid.load(std::memory_order_acquire));
  }

  if (mintxID == 0) return;
  else {
    //mintxIDから到達不能なバージョンを削除する
    Version *verTmp, *delTarget;
    for (unsigned int i = 0; i < TUPLE_NUM; ++i) {
      // 時間がかかるので，離脱条件チェック
      rsobject.End = rdtsc();
      if (chkClkSpan(rsobject.Bgn, rsobject.End, EXTIME * 1000 * 1000 * CLOCK_PER_US)) {
        Finish.store(true, std::memory_order_release);
        return;
      }
      // -----

      verTmp = Table[i].latest.load(memory_order_acquire);
      if (verTmp->status.load(memory_order_acquire) != VersionStatus::committed) 
        verTmp = verTmp->committed_prev;
      // この時点で， verTmp はコミット済み最新バージョン

      uint64_t verCstamp = verTmp->cstamp.load(memory_order_acquire);
      while (mintxID < (verCstamp >> 1)) {
        verTmp = verTmp->committed_prev;
        if (verTmp == nullptr) break;
        verCstamp = verTmp->cstamp.load(memory_order_acquire);
      }
      if (verTmp == nullptr) continue;
      // verTmp は mintxID によって到達可能．
      
      // ssn commit protocol によってverTmp->commited_prev までアクセスされる．
      verTmp = verTmp->committed_prev;
      if (verTmp == nullptr) continue;
      
      // verTmp->prev からガベコレ可能
      delTarget = verTmp->prev;
      if (delTarget == nullptr) continue;

      verTmp->prev = nullptr;
      while (delTarget != nullptr) {
        verTmp = delTarget->prev;
        delete delTarget;
        delTarget = verTmp;
      }
      //-----
    }
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
