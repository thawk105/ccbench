#pragma once

#include <iostream>
#include <set>
#include <vector>

#include "../../include/fileio.hh"
#include "../../include/procedure.hh"
#include "../../include/result.hh"
#include "../../include/string.hh"
#include "common.hh"
#include "log.hh"
#include "occ_op_element.hh"
#include "tuple.hh"

#define LOGSET_SIZE 1000

using namespace std;
using ReadSet = vector<ReadElement<Tuple>>;
using WriteSet = vector<WriteElement<Tuple>>;
using ProcedureSet = vector<Procedure>;

enum class TransactionStatus : uint8_t {
  kInFlight,
  kCommitted,
  kAborted,
};

class TxnExecutor {
 public:
  ReadSet read_set_;
  WriteSet write_set_;
  ProcedureSet pro_set_;

  int startTxId;

  vector<LogRecord> log_set_;
  LogHeader latest_log_header_;

  TransactionStatus status_;
  unsigned int thid_;
  /* lock_num_ ...
   * the number of locks in local write set.
   */
  Result* sres_;

  File logfile_;

  char write_val_[VAL_SIZE];
  char return_val_[VAL_SIZE];

  TxnExecutor(int thid, Result* sres);

  /**
   * @brief function about abort.
   * Clean-up local read/write set.
   * Release locks.
   * @return void
   */
  void abort();

  void begin();

  void displayWriteSet();

  Tuple* get_tuple(Tuple* table, uint64_t key) { return &table[key]; }

  /**
   * @brief Transaction read function.
   * @param [in] key The key of key-value
   */
  void read(uint64_t key);

  /**
   * @brief Search xxx set
   * @detail Search element of local set corresponding to given key.
   * In this prototype system, the value to be updated for each worker thread
   * is fixed for high performance, so it is only necessary to check the key
   * match.
   * @param Key [in] the key of key-value
   * @return Corresponding element of local set
   */
  ReadElement<Tuple>* searchReadSet(uint64_t key);

  /**
   * @brief Search xxx set
   * @detail Search element of local set corresponding to given key.
   * In this prototype system, the value to be updated for each worker thread
   * is fixed for high performance, so it is only necessary to check the key
   * match.
   * @param Key [in] the key of key-value
   * @return Corresponding element of local set
   */
  WriteElement<Tuple>* searchWriteSet(uint64_t key);

  bool validationPhase();

  void wal(uint64_t ctid);

  /**
   * @brief Transaction write function.
   * @param [in] key The key of key-value
   */
  void write(uint64_t key);

  void writePhase();

  void gc();
};
