#pragma once

#include <string.h>

#include <iostream>
#include <set>
#include <vector>

#include "../../include/inline.hh"
#include "../../include/procedure.hh"
#include "../../include/result.hh"
#include "../../include/string.hh"
#include "../../include/util.hh"
#include "common.hh"
#include "tictoc_op_element.hh"
#include "tuple.hh"

enum class TransactionStatus : uint8_t {
  inFlight,
  committed,
  aborted,
};

using namespace std;

extern void write_val_Generator(char *write_val_, size_t val_size, size_t thid);

class TxExecutor {
public:
  int thid_;
  uint64_t commit_ts_;
  uint64_t appro_commit_ts_;
  Result *tres_;
  bool wonly_ = false;
  vector <Procedure> pro_set_;

  TransactionStatus status_;
  vector <SetElement<Tuple>> read_set_;
  vector <SetElement<Tuple>> write_set_;

  char write_val_[VAL_SIZE];
  char return_val_[VAL_SIZE];

  TxExecutor(int thid, Result *tres);

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
  SetElement<Tuple> *searchWriteSet(uint64_t key);

  /**
   * @brief Search xxx set
   * @detail Search element of local set corresponding to given key.
   * In this prototype system, the value to be updated for each worker thread
   * is fixed for high performance, so it is only necessary to check the key
   * match.
   * @param Key [in] the key of key-value
   * @return Corresponding element of local set
   */
  SetElement<Tuple> *searchReadSet(uint64_t key);

  /**
   * @brief Transaction read function.
   * @param [in] key The key of key-value
   */
  void read(uint64_t key);

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
  void write(uint64_t key);

  /**
   * @brief write phase
   * @return void
   */
  void writePhase();
};
