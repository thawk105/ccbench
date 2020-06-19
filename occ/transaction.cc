#include <stdio.h>
#include <sys/time.h>
#include <xmmintrin.h>
#include <algorithm>
#include <bitset>
#include <fstream>
#include <sstream>
#include <string>

#include "include/atomic_tool.hh"
#include "include/common.hh"
#include "include/log.hh"
#include "include/transaction.hh"

#include "../include/backoff.hh"
#include "../include/debug.hh"
#include "../include/fileio.hh"
#include "../include/masstree_wrapper.hh"
#include "../include/tsc.hh"
#include "../include/util.hh"

extern void displayDB();

using namespace std;

uint64_t txId = 0;
vector<WriteSet> ws_list;

/* GC */
uint64_t deletedTxId = -1;
vector<int> progress;

TxnExecutor::TxnExecutor(int thid, Result *sres) : thid_(thid), sres_(sres) {
  read_set_.reserve(FLAGS_max_ope);
  write_set_.reserve(FLAGS_max_ope);
  pro_set_.reserve(FLAGS_max_ope);

  genStringRepeatedNumber(write_val_, VAL_SIZE, thid);
}

ReadElement<Tuple> *TxnExecutor::searchReadSet(uint64_t key) {
  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
    if ((*itr).key_ == key) return &(*itr);
  }

  return nullptr;
}

WriteElement<Tuple> *TxnExecutor::searchWriteSet(uint64_t key) {
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    if ((*itr).key_ == key) return &(*itr);
  }

  return nullptr;
}

void TxnExecutor::begin() {
  startTxId = loadAcquire(txId);
}

void TxnExecutor::read(uint64_t key) {
  if (searchReadSet(key) || searchWriteSet(key)) goto FINISH_READ;

  Tuple *tuple;
#if MASSTREE_USE
  tuple = MT.get_value(key);
#else
  tuple = get_tuple(Table, key);
#endif
  read_set_.emplace_back(key, tuple, tuple->val_);

FINISH_READ:
  return;
}

void TxnExecutor::write(uint64_t key) {
  if (searchWriteSet(key)) goto FINISH_WRITE;

  Tuple *tuple;
  ReadElement<Tuple> *re;
  re = searchReadSet(key);
  if (re) {
    tuple = re->rcdptr_;
  } else {
#if MASSTREE_USE
    tuple = MT.get_value(key);
#else
    tuple = get_tuple(Table, key);
#endif
  }
  write_set_.emplace_back(key, tuple);

FINISH_WRITE:
  return;
}

bool TxnExecutor::validationPhase() {
  int begin = startTxId - (loadAcquire(deletedTxId) + 1);
  int end = loadAcquire(txId) - (loadAcquire(deletedTxId) + 1);
  for (int i = begin; i < end; i++) {
    for (auto rItr = read_set_.begin(); rItr != read_set_.end(); ++rItr) {
      for (auto wItr = ws_list[i].begin(); wItr != ws_list[i].end(); ++wItr) {
        if ((*rItr).key_ == (*wItr).key_) {
          return false;
        }
      }
    }
  }
  return true;
}

void TxnExecutor::writePhase() {
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    memcpy((*itr).rcdptr_->val_, write_val_, VAL_SIZE);
  }
  ws_list.push_back(write_set_);
  read_set_.clear();
  write_set_.clear();
  progress[thid_] = loadAcquire(txId);
  storeRelease(txId, progress[thid_] + 1);
  gc();
}

void TxnExecutor::abort() {
  read_set_.clear();
  write_set_.clear();
}

void TxnExecutor::gc() {
  if (ws_list.size() > OCC_GC_THRESHOLD) {
    auto slowest = min_element(progress.begin(), progress.end());
    int numTxnsDeleted = *slowest - loadAcquire(deletedTxId);
    if (numTxnsDeleted > 0) {
      ws_list.erase(ws_list.begin(), ws_list.begin() + numTxnsDeleted);
      storeRelease(deletedTxId, *slowest);
    }
  }
}

void TxnExecutor::wal(uint64_t ctid) {
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    LogRecord log(ctid, (*itr).key_, write_val_);
    log_set_.emplace_back(log);
    latest_log_header_.chkSum_ += log.computeChkSum();
    ++latest_log_header_.logRecNum_;
  }

  if (log_set_.size() > LOGSET_SIZE / 2) {
    // prepare write header
    latest_log_header_.convertChkSumIntoComplementOnTwo();

    // write header
    logfile_.write((void *)&latest_log_header_, sizeof(LogHeader));

    // write log record
    // for (auto itr = log_set_.begin(); itr != log_set_.end(); ++itr)
    //  logfile_.write((void *)&(*itr), sizeof(LogRecord));
    logfile_.write((void *)&(log_set_[0]),
                   sizeof(LogRecord) * latest_log_header_.logRecNum_);

    // logfile_.fdatasync();

    // clear for next transactions.
    latest_log_header_.init();
    log_set_.clear();
  }
}
