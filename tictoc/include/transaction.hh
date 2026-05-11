#pragma once

#include <string.h>

#include <iostream>
#include <set>
#include <vector>

#include "../../include/backoff.hh"
#include "../../include/inline.hh"
#include "../../include/procedure.hh"
#include "../../include/result.hh"
#include "../../include/status.hh"
#include "../../include/string.hh"
#include "../../include/util.hh"
#include "common.hh"
#include "scan_callback.hh"
#include "tictoc_op_element.hh"
#include "tuple.hh"

enum class TransactionStatus : uint8_t {
  invalid,
  inflight,
  committed,
  aborted,
};

using namespace std;

extern void write_val_Generator(char *write_val_, size_t val_size, size_t thid);

class TxScanCallback;

class TxExecutor {
public:
  int thid_;
  uint64_t commit_ts_;
  uint64_t appro_commit_ts_;
  Result *result_;
  Backoff backoff_;
  const bool& quit_; // for thread termination control
  bool reconnoitering_ = false;
  bool is_batch_ = false;
  bool is_ronly_ = false;
  bool is_wonly_ = false;
  vector <Procedure> pro_set_;

  TransactionStatus status_;
  vector <SetElement<Tuple>> read_set_;
  vector <SetElement<Tuple>> write_set_;
  std::deque <GCElement<Tuple>> gc_records_;
  std::unordered_map<void*, uint64_t> node_map_;
  uint64_t epoch_timer_start, epoch_timer_stop;
  TxScanCallback callback_;

  TxExecutor(int thid, Result *res, const bool &quit)
      : result_(res), thid_(thid), quit_(quit), backoff_(FLAGS_clocks_per_us),
        callback_(TxScanCallback(this)) {
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

  /**
   * @brief Initialize function of transaction.
   * @return void
   */
  void begin();

  /**
   * @brief display write set contents.
   * @return void
   */
  void dispWS();

  Tuple *get_tuple(Tuple *table, uint64_t key) { return &table[key]; }

  /**
   * @brief lock records in local write set.
   * @return void
   */
  void lockWriteSet();

  /**
   * @brief Early abort.
   * It is decided that the record is locked.
   * Check whether it can read old state.
   * If it can, it succeeds read old state and
   * is going to be serialized between old state and new state.
   * @param [in] v1 timestamp of record.
   * @return true early abort
   * @return false not abort
   */
  bool preemptiveAborts(const TsWord &v1);

  /**
   * @brief Search xxx set
   * @detail Search element of local set corresponding to given key.
   * In this prototype system, the value to be updated for each worker thread
   * is fixed for high performance, so it is only necessary to check the key
   * match.
   * @param Key [in] the key of key-value
   * @return Corresponding element of local set
   */
  SetElement<Tuple> *searchWriteSet(Storage s, std::string_view key);

  /**
   * @brief Search xxx set
   * @detail Search element of local set corresponding to given key.
   * In this prototype system, the value to be updated for each worker thread
   * is fixed for high performance, so it is only necessary to check the key
   * match.
   * @param Key [in] the key of key-value
   * @return Corresponding element of local set
   */
  SetElement<Tuple> *searchReadSet(Storage s, std::string_view key);

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
   * @brief unlock all elements of write set.
   * @return void
   */
  void unlockWriteSet();

  /**
   * @brief unlock partial elements of write set.
   * @return void
   */
  void unlockWriteSet(std::vector<SetElement<Tuple>>::iterator end);

  bool validationPhase();

  /**
   * @brief Transaction write function.
   * @param [in] key The key of key-value
   */
  Status write(Storage s, std::string_view key, TupleBody&& body);

  Status insert(Storage s, std::string_view key, TupleBody&& body);

  Status delete_record(Storage s, std::string_view key);

  /**
   * @brief write phase
   * @return void
   */
  void writePhase();

  bool commit();

  void gc_records();

  void reconnoiter_begin(); 
  void reconnoiter_end(); 

  bool isLeader();

  void leaderWork();
};
