#pragma once

#include <stdio.h>
#include <atomic>
#include <iostream>
#include <map>
#include <queue>

#include "/home/tanabe/package/tbb/include/tbb/scalable_allocator.h"

#include "../../include/debug.hh"
#include "../../include/inline.hh"
#include "../../include/procedure.hh"
#include "../../include/string.hh"
#include "../../include/util.hh"
#include "cicada_op_element.hh"
#include "common.hh"
#include "tuple.hh"
#include "time_stamp.hh"
#include "version.hh"
#include "result.hh"

enum class TransactionStatus : uint8_t {
  invalid,
  inflight,
  commit,
  abort,
};


class TxExecutor {
public:
  TransactionStatus status_ = TransactionStatus::invalid;
  TimeStamp wts_;
  std::vector<ReadElement<Tuple>, tbb::scalable_allocator<ReadElement<Tuple>>> read_set_;
  std::vector<WriteElement<Tuple>, tbb::scalable_allocator<WriteElement<Tuple>>> write_set_;
  std::deque<GCElement<Tuple>, tbb::scalable_allocator<GCElement<Tuple>>> gcq_;
  std::vector<Procedure> pro_set_;
  CicadaResult* cres_ = nullptr;

  bool ronly_;
  uint8_t thid_ = 0;
  uint64_t rts_;
  uint64_t start_, stop_; // for one-sided synchronization
  uint64_t grpcmt_start_, grpcmt_stop_; // for group commit
  uint64_t gcstart_, gcstop_; // for garbage collection
  uint64_t continuing_commit_ = 0;

  char return_val_[VAL_SIZE] = {};
  char write_val_[VAL_SIZE] = {};

  TxExecutor(uint8_t thid, CicadaResult* cres) : cres_(cres), thid_(thid) {
    // wait to initialize MinWts
    while(MinWts.load(memory_order_acquire) == 0);
    rts_ = MinWts.load(memory_order_acquire) - 1;
    wts_.generateTimeStampFirst(thid_);

    __atomic_store_n(&(ThreadWtsArray[thid_].obj_), wts_.ts_, __ATOMIC_RELEASE);
    unsigned int expected, desired;
    expected = FirstAllocateTimestamp.load(memory_order_acquire);
    for (;;) {
      desired = expected + 1;
      if (FirstAllocateTimestamp.compare_exchange_weak(expected, desired, memory_order_acq_rel)) break;
    }

    read_set_.reserve(MAX_OPE);
    write_set_.reserve(MAX_OPE);
    pro_set_.reserve(MAX_OPE);
    //gcq.resize(MAX_OPE);

    genStringRepeatedNumber(write_val_, VAL_SIZE, thid_);

    start_ = rdtscp();
    gcstart_ = start_;
  }

  void tbegin();
  char* tread(uint64_t key);
  void twrite(uint64_t key);
  bool validation();
  void writePhase();
  void swal();
  void pwal();  //parallel write ahead log.
  void precpv();  //pre-commit pending versions
  void cpv(); //commit pending versions
  void gcpv();  //group commit pending versions
  bool chkGcpvTimeout();
  void earlyAbort();
  void abort();
  void wSetClean();
  void displayWset();
  void mainte();  //maintenance

  static INLINE Tuple* get_tuple(Tuple *table, uint64_t key) {
    return &table[key];
  }
};
