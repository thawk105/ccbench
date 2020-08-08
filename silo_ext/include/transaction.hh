#pragma once

#include <iostream>
#include <set>
#include <string_view>
#include <vector>

#include "../../include/fileio.hh"
#include "../../include/procedure.hh"
#include "../../include/result.hh"
#include "../../include/string.hh"
#include "local_set_element.hh"
#include "tuple.hh"

namespace silo_ext {

enum class TransactionStatus : uint8_t {
  kInFlight,
  kCommitted,
  kAborted,
};

class TxnExecutor {
public:
  std::vector<ReadElement<Tuple>> read_set_;
  std::vector<WriteElement<Tuple>> write_set_;

  TransactionStatus status_;
  std::size_t thid_;
  /* lock_num_ ...
   * the number of locks in local write set.
   */
  Result *sres_;

  Tidword mrctid_;
  Tidword max_rset_, max_wset_;

  TxnExecutor(int thid, Result *sres);

  /**
   * @brief function about abort.
   * Clean-up local read/write set.
   * Release locks.
   * @return void
   */
  void abort();

  void begin();

  void tx_delete(std::string_view key);

  void displayWriteSet();

  void insert(std::string_view key, std::string_view val); // NOLINT

  void lockWriteSet();

  /**
   * @brief Transaction read function.
   * @param [in] key The key of key-value
   */
  void read(std::string_view key);

  /**
   * @brief Search xxx set
   * @detail Search element of local set corresponding to given key.
   * In this prototype system, the value to be updated for each worker thread
   * is fixed for high performance, so it is only necessary to check the key
   * match.
   * @param Key [in] the key of key-value
   * @return Corresponding element of local set
   */
  ReadElement<Tuple> *searchReadSet(std::string_view key);

  /**
   * @brief Search xxx set
   * @detail Search element of local set corresponding to given key.
   * In this prototype system, the value to be updated for each worker thread
   * is fixed for high performance, so it is only necessary to check the key
   * match.
   * @param Key [in] the key of key-value
   * @return Corresponding element of local set
   */
  WriteElement<Tuple> *searchWriteSet(std::string_view key);

  void unlockWriteSet();

  void unlockWriteSet(std::vector<WriteElement<Tuple>>::iterator end);

  bool validationPhase();

  /**
   * @brief Transaction write function.
   * @param [in] key The key of key-value
   */
  void write(std::string_view, std::string_view val);

  void writePhase();
};

} // namespace silo
