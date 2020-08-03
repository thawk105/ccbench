#pragma once

#include <iostream>
#include <set>
#include <string_view>
#include <vector>

#include "../../include/fileio.hh"
#include "../../include/procedure.hh"
#include "../../include/result.hh"
#include "../../include/string.hh"
#include "common.hh"
#include "log.hh"
#include "silo_op_element.hh"
#include "tuple.hh"

#define LOGSET_SIZE 1000

enum class TransactionStatus : uint8_t {
  kInFlight,
  kCommitted,
  kAborted,
};

class TxnExecutor {
public:
  std::vector<ReadElement<Tuple>> read_set_;
  std::vector<WriteElement<Tuple>> write_set_;
  std::vector<Procedure> pro_set_;

  std::vector<LogRecord> log_set_;
  LogHeader latest_log_header_;

  TransactionStatus status_;
  unsigned int thid_;
  /* lock_num_ ...
   * the number of locks in local write set.
   */
  Result *sres_;

  File logfile_;

  Tidword mrctid_;
  Tidword max_rset_, max_wset_;

  char write_val_[VAL_SIZE];
  // used by fast approach for benchmark
  char return_val_[VAL_SIZE];

  TxnExecutor(int thid, Result *sres);

  /**
   * @brief function about abort.
   * Clean-up local read/write set.
   * Release locks.
   * @return void
   */
  void abort();

  void begin();

  void tx_delete(std::uint64_t key);

  void displayWriteSet();

  Tuple *get_tuple(Tuple *table, std::uint64_t key) { return &table[key]; }

  void insert([[maybe_unused]]std::uint64_t key, [[maybe_unused]]std::string_view val = ""); // NOLINT

  void lockWriteSet();

  /**
   * @brief Transaction read function.
   * @param [in] key The key of key-value
   */
  void read(std::uint64_t key);

  /**
   * @brief Search xxx set
   * @detail Search element of local set corresponding to given key.
   * In this prototype system, the value to be updated for each worker thread
   * is fixed for high performance, so it is only necessary to check the key
   * match.
   * @param Key [in] the key of key-value
   * @return Corresponding element of local set
   */
  ReadElement<Tuple> *searchReadSet(std::uint64_t key);

  /**
   * @brief Search xxx set
   * @detail Search element of local set corresponding to given key.
   * In this prototype system, the value to be updated for each worker thread
   * is fixed for high performance, so it is only necessary to check the key
   * match.
   * @param Key [in] the key of key-value
   * @return Corresponding element of local set
   */
  WriteElement<Tuple> *searchWriteSet(std::uint64_t key);

  void unlockWriteSet();

  void unlockWriteSet(std::vector<WriteElement<Tuple>>::iterator end);

  bool validationPhase();

  void wal(std::uint64_t ctid);

  /**
   * @brief Transaction write function.
   * @param [in] key The key of key-value
   */
  void write(std::uint64_t key, std::string_view val = "");

  void writePhase();
};
