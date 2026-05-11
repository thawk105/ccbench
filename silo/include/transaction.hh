#pragma once

#include <iostream>
#include <set>
#include <string_view>
#include <vector>

#include "../../include/backoff.hh"
#include "../../include/fileio.hh"
#include "../../include/procedure.hh"
#include "../../include/result.hh"
#include "../../include/status.hh"
#include "../../include/string.hh"
#include "../../include/workload.hh"
#include "common.hh"
#include "log.hh"
#include "silo_op_element.hh"
#include "scan_callback.hh"
#include "tuple.hh"

#define LOGSET_SIZE 1000

enum class TransactionStatus : uint8_t {
  invalid,
  inflight,
  committed,
  aborted,
};

class TxScanCallback;

class TxExecutor {
public:
  std::vector<ReadElement<Tuple>> read_set_;
  std::vector<WriteElement<Tuple>> write_set_;
  std::vector<Procedure> pro_set_;
  std::deque<Tuple*> gc_records_;
  std::unordered_map<void*, uint64_t> node_map_;

  std::vector<LogRecord> log_set_;
  LogHeader latest_log_header_;

  TransactionStatus status_;
  size_t thid_;
  /* lock_num_ ...
   * the number of locks in local write set.
   */
  Result *result_;
  uint64_t epoch_timer_start, epoch_timer_stop;
  Backoff backoff_;
  const bool& quit_; // for thread termination control
  TxScanCallback callback_;

  File logfile_;

  Tidword mrctid_;
  Tidword max_rset_, max_wset_;

  bool reconnoitering_ = false;
  bool is_ronly_ = false;
  bool is_batch_ = false;

  // char write_val_[VAL_SIZE];
  // // used by fast approach for benchmark
  // char return_val_[VAL_SIZE];

  TxExecutor(int thid, Result *res, const bool &quit)
    : result_(res), thid_(thid), quit_(quit), backoff_(FLAGS_clocks_per_us),
      callback_(TxScanCallback(this)) {
    // latest_log_header_.init();
    max_rset_.obj_ = 0;
    max_wset_.obj_ = 0;
    epoch_timer_start = rdtsc();
    epoch_timer_stop = 0;
  }

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

  Status insert(Storage s, std::string_view key, TupleBody&& body);

  Status delete_record(Storage s, std::string_view key);

  void lockWriteSet();

  /**
   * @brief Transaction read function.
   * @param [in] key The key of key-value
   */
  Status read(Storage s, std::string_view key, TupleBody** body);

  Status read_internal(Storage s, std::string_view key, Tuple* tuple);

  Status scan(Storage s,
              std::string_view left_key, bool l_exclusive,
              std::string_view right_key, bool r_exclusive,
              std::vector<TupleBody *>&result);

  Status scan(Storage s,
              std::string_view left_key, bool l_exclusive,
              std::string_view right_key, bool r_exclusive,
              std::vector<TupleBody *>&result, int64_t limit);

  /**
   * @brief Search xxx set
   * @detail Search element of local set corresponding to given key.
   * In this prototype system, the value to be updated for each worker thread
   * is fixed for high performance, so it is only necessary to check the key
   * match.
   * @param Key [in] the key of key-value
   * @return Corresponding element of local set
   */
  ReadElement<Tuple> *searchReadSet(Storage s, std::string_view key);

  /**
   * @brief Search xxx set
   * @detail Search element of local set corresponding to given key.
   * In this prototype system, the value to be updated for each worker thread
   * is fixed for high performance, so it is only necessary to check the key
   * match.
   * @param Key [in] the key of key-value
   * @return Corresponding element of local set
   */
  WriteElement<Tuple> *searchWriteSet(Storage s, std::string_view key);

  void unlockWriteSet();

  void unlockWriteSet(std::vector<WriteElement<Tuple>>::iterator end);

  bool validationPhase();

  void wal(std::uint64_t ctid);

  /**
   * @brief Transaction write function.
   * @param [in] key The key of key-value
   */
  Status write(Storage s, std::string_view key, TupleBody&& body);

  void writePhase();

  bool commit();

  void reconnoiter_begin(); 
  void reconnoiter_end(); 

  bool isLeader();

  void leaderWork();

  void gc_records();
};
