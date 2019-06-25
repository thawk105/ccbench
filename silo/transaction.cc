#include <algorithm>
#include <bitset>
#include <fstream>
#include <sstream>
#include <stdio.h>
#include <string>
#include <sys/time.h>
#include <xmmintrin.h>

#include "include/atomic_tool.hpp"
#include "include/common.hpp"
#include "include/log.hpp"
#include "include/transaction.hpp"

#include "../include/debug.hpp"
#include "../include/fileio.hpp"
#include "../include/masstree_wrapper.hpp"

extern bool chkSpan(struct timeval &start, struct timeval &stop, long threshold);
extern void displayDB();

using namespace std;

void
TxnExecutor::tbegin()
{
  max_wset.obj = 0;
  max_rset.obj = 0;
}

WriteElement<Tuple> *
TxnExecutor::searchWriteSet(uint64_t key)
{
  for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
    if ((*itr).key == key) return &(*itr);
  }

  return nullptr;
}

ReadElement<Tuple> *
TxnExecutor::searchReadSet(uint64_t key)
{
  for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
    if ((*itr).key == key) return &(*itr);
  }

  return nullptr;
}

char*
TxnExecutor::tread(uint64_t key)
{
#if MASSTREE_USE
  Tuple *tuple = MT.get_value(key);
#else
  Tuple *tuple = get_tuple(Table, key);
#endif

  //w
  WriteElement<Tuple> *inW = searchWriteSet(key);
  if (inW) return writeVal;

  //r
  ReadElement<Tuple> *inR = searchReadSet(key);
  if (inR) return inR->val;

  
  //(a) reads the TID word, spinning until the lock is clear
  Tidword expected, check;

  expected.obj = __atomic_load_n(&(tuple->tidword.obj), __ATOMIC_ACQUIRE);
  //check if it is locked.
  //spinning until the lock is clear
  
  for (;;) {
    while (expected.lock) {
      expected.obj = __atomic_load_n(&(tuple->tidword.obj), __ATOMIC_ACQUIRE);
    }
    
    //(b) checks whether the record is the latest version
    // omit. because this is implemented by single version
    
    //(c) reads the data
    memcpy(returnVal, tuple->val, VAL_SIZE);

    //(d) performs a memory fence
    // don't need.
    // order of load don't exchange.
    
    //(e) checks the TID word again
    check.obj = __atomic_load_n(&(tuple->tidword.obj), __ATOMIC_ACQUIRE);
    if (expected == check) {
      break;
    } else {
      expected = check;
      ++rsob->local_extra_reads;
    }
  }
  
  readSet.emplace_back(key, tuple, returnVal, expected); // emplace の方が性能が良い
  return returnVal;
}

void 
TxnExecutor::twrite(uint64_t key)
{
  WriteElement<Tuple> *inW = searchWriteSet(key);
  if (inW) return;

#if MASSTREE_USE
  Tuple *tuple = MT.get_value(key);
#else
  Tuple *tuple = get_tuple(Table, key);
#endif

  writeSet.emplace_back(key, tuple); // push の方が性能が良い
  return;
}

bool 
TxnExecutor::validationPhase()
{
  /* Phase 1 
   * lock writeSet sorted.*/
  sort(writeSet.begin(), writeSet.end());
  lockWriteSet();

  asm volatile("" ::: "memory");
  atomicStoreThLocalEpoch(thid, atomicLoadGE());
  asm volatile("" ::: "memory");

  /* Phase 2 abort if any condition of below is satisfied. 
   * 1. tid of readSet changed from it that was got in Read Phase.
   * 2. not latest version
   * 3. the tuple is locked and it isn't included by its write set.*/
  
  Tidword check;
  for (auto itr = readSet.begin(); itr != readSet.end(); ++itr) {
    //1
    check.obj = __atomic_load_n(&((*itr).rcdptr->tidword.obj), __ATOMIC_ACQUIRE);
    if ((*itr).tidword.epoch != check.epoch || (*itr).tidword.tid != check.tid) {
      return false;
    }
    //2
    //if (!check.latest) return false;

    //3
    if (check.lock && !searchWriteSet((*itr).key)) return false;
    max_rset = max(max_rset, check);
  }

  //goto Phase 3
  return true;
}

void TxnExecutor::abort() 
{
  unlockWriteSet();

  readSet.clear();
  writeSet.clear();
}

void TxnExecutor::wal(uint64_t ctid)
{
  for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
    LogRecord log(ctid, (*itr).key, writeVal);
    logSet.push_back(log);
    latestLogHeader.chkSum += log.computeChkSum();
    ++latestLogHeader.logRecNum;
  }

  if (logSet.size() > LOGSET_SIZE / 2) {
    // prepare write header
    latestLogHeader.convertChkSumIntoComplementOnTwo();

    // write header
    logfile.write((void *)&latestLogHeader, sizeof(LogHeader));

    // write log record
    //for (auto itr = logSet.begin(); itr != logSet.end(); ++itr)
    //  logfile.write((void *)&(*itr), sizeof(LogRecord));
    logfile.write((void *)&(logSet[0]), sizeof(LogRecord) * latestLogHeader.logRecNum);
    
    //logfile.fdatasync();

    // clear for next transactions.
    latestLogHeader.init();
    logSet.clear();
  }
}

void TxnExecutor::writePhase()
{
  //It calculates the smallest number that is 
  //(a) larger than the TID of any record read or written by the transaction,
  //(b) larger than the worker's most recently chosen TID,
  //and (C) in the current global epoch.
  
  Tidword tid_a, tid_b, tid_c;

  //calculates (a)
  //about readSet
  tid_a = max(max_wset, max_rset);
  tid_a.tid++;
  
  //calculates (b)
  //larger than the worker's most recently chosen TID,
  tid_b = mrctid;
  tid_b.tid++;

  //calculates (c)
  tid_c.epoch = ThLocalEpoch[thid].obj;

  //compare a, b, c
  Tidword maxtid =  max({tid_a, tid_b, tid_c});
  maxtid.lock = 0;
  maxtid.latest = 1;
  mrctid = maxtid;

  //wal(maxtid.obj);

  //write(record, commit-tid)
  for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
    //update and unlock
    memcpy((*itr).rcdptr->val, writeVal, VAL_SIZE);
    __atomic_store_n(&((*itr).rcdptr->tidword.obj), maxtid.obj, __ATOMIC_RELEASE);
  }

  readSet.clear();
  writeSet.clear();
}

void TxnExecutor::lockWriteSet()
{
  Tidword expected, desired;

  for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
    expected.obj = __atomic_load_n(&((*itr).rcdptr->tidword.obj), __ATOMIC_ACQUIRE);
    for (;;) {
      if (expected.lock) {
        expected.obj = __atomic_load_n(&((*itr).rcdptr->tidword.obj), __ATOMIC_ACQUIRE);
      } else {
        desired = expected;
        desired.lock = 1;
        if (__atomic_compare_exchange_n(&((*itr).rcdptr->tidword.obj), &(expected.obj), desired.obj, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) break;
      }
    }

    max_wset = max(max_wset, expected);
  }
}

void TxnExecutor::unlockWriteSet()
{
  Tidword expected, desired;

  for (auto itr = writeSet.begin(); itr != writeSet.end(); ++itr) {
    expected.obj = __atomic_load_n(&((*itr).rcdptr->tidword.obj), __ATOMIC_ACQUIRE);
    desired = expected;
    desired.lock = 0;
    __atomic_store_n(&((*itr).rcdptr->tidword.obj), desired.obj, __ATOMIC_RELEASE);
  }
}

void
TxnExecutor::display_write_set()
{
  printf("display_write_set()\n");
  printf("--------------------\n");
  for (auto& ws : writeSet) {
    printf("key\t:\t%lu\n", ws.key);
  }
}
