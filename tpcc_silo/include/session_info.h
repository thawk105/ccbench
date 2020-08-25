/**
 * @file session_info.h
 * @brief private scheme of transaction engine
 */

#pragma once

#include <pthread.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <map>
#include <tuple>
#include <utility>
#include <vector>

#include "compiler.h"
#include "cpu.h"
#include "fileio.h"
#include "log.h"
#include "record.h"
#include "scheme.h"
#include "tid.h"

#include "scheme_global.h"
#include "garbage_collection.h"

namespace ccbench {

class session_info {
public:
  using RecPtrContainer = garbage_collection::RecPtrContainer;
  using ObjEpochContainer = garbage_collection::ObjEpochContainer;
  using ObjEpochInfo = garbage_collection::ObjEpochInfo;

  class gc_handler {
  public:
    std::size_t get_container_index() const {  // NOLINT
      return container_index_;
    }

    RecPtrContainer& get_record_container() const {  // NOLINT
      return *record_container_;
    }

    [[nodiscard]] ObjEpochContainer& get_value_container() const {  // NOLINT
      return *value_container_;
    }

    void set_container_index(std::size_t index) { container_index_ = index; }

    void set_record_container(RecPtrContainer *cont) {
      record_container_ = cont;
    }

    void set_value_container(ObjEpochContainer *cont) {
      value_container_ = cont;
    }

  private:
    std::size_t container_index_{};  // common to record and value;
    RecPtrContainer *record_container_{};
    ObjEpochContainer *value_container_{};
  };

  class scan_handler {
  public:
    [[maybe_unused]] std::map<ScanHandle, std::size_t> &
    get_len_rkey() {  // NOLINT
      return len_rkey_;
    }

    [[maybe_unused]] std::map<ScanHandle, bool> &get_r_exclusive_() {  // NOLINT
      return r_exclusive_;
    }

    [[maybe_unused]] std::map<ScanHandle, std::unique_ptr<char[]>> &  // NOLINT
    get_rkey() {                                                     // NOLINT
      return rkey_;
    }

    std::map<ScanHandle, std::vector<const Record *>> &get_scan_cache() {
      return scan_cache_;
    }

    [[maybe_unused]] std::map<ScanHandle, std::size_t> &
    get_scan_cache_itr() {  // NOLINT
      return scan_cache_itr_;
    }

  private:
    std::map<ScanHandle, std::size_t> len_rkey_{};
    std::map<ScanHandle, bool> r_exclusive_{};
    std::map<ScanHandle, std::unique_ptr<char[]>> rkey_{};  // NOLINT
    std::map<ScanHandle, std::vector<const Record *>> scan_cache_{};
    std::map<ScanHandle, std::size_t> scan_cache_itr_{};
  };

  class log_handler {
  public:
    std::vector<Log::LogRecord> &get_log_set() { return log_set_; }  // NOLINT

    File &get_log_file() { return log_file_; }  // NOLINT

    Log::LogHeader &get_latest_log_header() {  // NOLINT
      return latest_log_header_;
    }

    void set_log_dir(std::string &&str) { log_dir_ = str; }

  private:
    std::string log_dir_{};
    File log_file_{};
    std::vector<Log::LogRecord> log_set_{};
    Log::LogHeader latest_log_header_{};
  };

  explicit session_info(Token token) {
    this->token_ = token;
    get_mrctid().reset();
  }

  session_info() {
    this->visible_.store(false, std::memory_order_release);
    get_mrctid().reset();
    log_handle_.set_log_dir(MAC2STR(PROJECT_ROOT));
  }

  /**
   * @brief clean up about holding operation info.
   */
  void clean_up_ops_set();

  /**
   * @brief clean up about scan operation.
   */
  void clean_up_scan_caches();

  /**
   * @brief for debug.
   */
  [[maybe_unused]] void display_read_set();

  [[maybe_unused]] void display_write_set();

  bool cas_visible(bool &expected, bool &desired) {  // NOLINT
    return visible_.compare_exchange_strong(expected, desired,
                                            std::memory_order_acq_rel);
  }

  /**
   * @brief check whether it already executed update or insert operation.
   * @param[in] key the key of record.
   * @pre this function is only executed in delete_record operation.
   * @return Status::OK no update/insert before this delete_record operation.
   * @return Status::WARN_CANCEL_PREVIOUS_OPERATION it canceled an update/insert
   * operation before this delete_record operation.
   */
  Status check_delete_after_write(Storage st, std::string_view key);  // NOLINT

  void gc_records_and_values() const;

  [[nodiscard]] epoch::epoch_t get_epoch() const {  // NOLINT
    return epoch_.load(std::memory_order_acquire);
  }

  [[maybe_unused]] std::size_t get_gc_container_index() {  // NOLINT
    return gc_handle_.get_container_index();
  }

  RecPtrContainer& get_gc_record_container() {  // NOLINT
    return gc_handle_.get_record_container();
  }

  ObjEpochContainer& get_gc_value_container() {  // NOLINT
    return gc_handle_.get_value_container();
  }

  std::map<ScanHandle, std::size_t> &get_len_rkey() {  // NOLINT
    return scan_handle_.get_len_rkey();
  }

  std::vector<Log::LogRecord> &get_log_set() {  // NOLINT
    return log_handle_.get_log_set();
  }

  tid_word &get_mrctid() { return mrc_tid_; }  // NOLINT

  std::map<ScanHandle, bool> &get_r_exclusive() {  // NOLINT
    return scan_handle_.get_r_exclusive_();
  }

  std::map<ScanHandle, std::unique_ptr<char[]>> &get_rkey() {  // NOLINT
    return scan_handle_.get_rkey();
  }

  std::vector<read_set_obj> &get_read_set() {  // NOLINT
    return read_set;
  }

  std::map<ScanHandle, std::vector<const Record *>> &
  get_scan_cache() {  // NOLINT
    return scan_handle_.get_scan_cache();
  }

  std::map<ScanHandle, std::size_t> &get_scan_cache_itr() {  // NOLINT
    return scan_handle_.get_scan_cache_itr();
  }

  [[maybe_unused]] Token &get_token() { return token_; }  // NOLINT

  [[maybe_unused]] [[nodiscard]] const Token &get_token() const {  // NOLINT
    return token_;
  }

  bool &get_txbegan() { return tx_began_; }  // NOLINT

  [[maybe_unused]] [[nodiscard]] const bool &get_txbegan() const {  // NOLINT
    return tx_began_;
  }  // NOLINT

  [[nodiscard]] bool get_visible() const {  // NOLINT
    return visible_.load(std::memory_order_acquire);
  }

  std::vector<write_set_obj> &get_write_set() {  // NOLINT
    return write_set;
  }

  /**
   * @brief Remove inserted records of write set from masstree.
   *
   * Insert operation inserts records to masstree in read phase.
   * If the transaction is aborted, the records exists for ever with absent
   * state. So it needs to remove the inserted records of write set from
   * masstree at abort.
   * @pre This function is called at abort.
   */
  void remove_inserted_records_of_write_set_from_masstree();

  /**
   * @brief check whether it already executed search operation.
   * @param[in] key the key of record.
   * @return the pointer of element. If it is nullptr, it is not found.
   */
  read_set_obj *search_read_set(Storage storage, std::string_view key);  // NOLINT

  /**
   * @brief check whether it already executed search operation.
   * @param [in] rec_ptr the pointer of record.
   * @return the pointer of element. If it is nullptr, it is not found.
   */
  read_set_obj *search_read_set(const Record *rec_ptr);  // NOLINT

  /**
   * @brief check whether it already executed write operation.
   * @param [in] key the key of record.
   * @return the pointer of element. If it is nullptr, it is not found.
   */
  write_set_obj *search_write_set(Storage storage, std::string_view key);  // NOLINT

  /**
   * @brief check whether it already executed update/insert operation.
   * @param [in] rec_ptr the pointer of record.
   * @return the pointer of element. If it is nullptr, it is not found.
   */
  const write_set_obj *search_write_set(const Record *rec_ptr);  // NOLINT

  /**
   * @brief unlock records in write set.
   *
   * This function unlocked all records in write set absolutely.
   * So it has a pre-condition.
   * @pre It has locked all records in write set.
   * @return void
   */
  void unlock_write_set();

  /**
   * @brief unlock write set object between @a begin and @a end.
   * @param [in] begin Starting points.
   * @param [in] end Ending points.
   * @pre It already locked write set between @a begin and @a end.
   * @return void
   */
  void unlock_write_set(std::vector<write_set_obj>::iterator begin,
                        std::vector<write_set_obj>::iterator end);

  /**
   * @brief write-ahead logging
   * @param [in] commit_id commit tid.
   * @return void
   */
  void wal(uint64_t commit_id);

  [[maybe_unused]] void set_token(Token token) { token_ = token; }

  void set_epoch(epoch::epoch_t epoch) {
    epoch_.store(epoch, std::memory_order_release);
  }

  void set_gc_container_index(std::size_t new_index) {
    gc_handle_.set_container_index(new_index);
  }

  void set_gc_record_container(RecPtrContainer *cont) {  // NOLINT
    gc_handle_.set_record_container(cont);
  }

  void set_gc_value_container(ObjEpochContainer *cont) {  // NOLINT
    gc_handle_.set_value_container(cont);
  }

  void set_mrc_tid(const tid_word &tid) { mrc_tid_ = tid; }

  void set_tx_began(bool tf) { tx_began_ = tf; }

  void set_visible(bool visible) {
    visible_.store(visible, std::memory_order_release);
  }

private:
  alignas(CACHE_LINE_SIZE) Token token_{};
  std::atomic<epoch::epoch_t> epoch_{};
  tid_word mrc_tid_{};  // most recently chosen tid, for calculate new tids.
  std::atomic<bool> visible_{};
  bool tx_began_{};

  /**
   * about garbage collection
   */
  gc_handler gc_handle_;

  /**
   * about holding operation info.
   */
  std::vector<read_set_obj> read_set{};
  std::vector<write_set_obj> write_set{};

  /**
   * about scan operation.
   */
  scan_handler scan_handle_;
  /**
   * about logging.
   */
  log_handler log_handle_;
};

}  // namespace ccbench
