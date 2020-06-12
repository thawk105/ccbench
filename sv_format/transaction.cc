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

TxnExecutor::TxnExecutor(int thid, Result *sres) : thid_(thid), sres_(sres) {
  read_set_.reserve(FLAGS_max_ope);
  write_set_.reserve(FLAGS_max_ope);
  pro_set_.reserve(FLAGS_max_ope);

  genStringRepeatedNumber(write_val_, VAL_SIZE, thid);
}

void TxnExecutor::begin() {
}

WriteElement<Tuple> *TxnExecutor::searchWriteSet(uint64_t key) {
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    if ((*itr).key_ == key) return &(*itr);
  }

  return nullptr;
}

ReadElement<Tuple> *TxnExecutor::searchReadSet(uint64_t key) {
  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
    if ((*itr).key_ == key) return &(*itr);
  }

  return nullptr;
}

void TxnExecutor::read(uint64_t key) {
}

void TxnExecutor::write(uint64_t key) {
}

bool TxnExecutor::validationPhase() {
}

void TxnExecutor::abort() {
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

void TxnExecutor::writePhase() {
}

