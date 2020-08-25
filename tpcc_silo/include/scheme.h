/**
 * @file cc/silo_variant/include/scheme.h
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
#include "scheme_global.h"
#include "log.h"
#include "record.h"
#include "scheme.h"
#include "tid.h"

namespace ccbench {

/**
 * @brief element of write set.
 * @details copy constructor/assign operator can't be used in this class
 * in terms of performance.
 */
class write_set_obj {  // NOLINT
public:
  // for insert/delete operation
  write_set_obj(OP_TYPE op, Storage st, Record *rec_ptr) : op_(op), st_(st), rec_ptr_(rec_ptr) {}

#if 0 // QQQ
  // for update/upsert
  write_set_obj(std::string_view key, std::string_view val, std::align_val_t val_align, const OP_TYPE op, Storage st,
                Record* rec_ptr)
          : op_(op), st_(st),
            rec_ptr_(rec_ptr),
            tuple_(key, val, val_align) {}
#endif
  // for update/upsert
  write_set_obj(OP_TYPE op, Storage st, Record* rec_ptr, Tuple&& tuple)
    : op_(op), st_(st), rec_ptr_(rec_ptr), tuple_(std::move(tuple)) {
  }

  write_set_obj(const write_set_obj &right) = delete;

  // for std::sort
  write_set_obj(write_set_obj &&right) = default;

  write_set_obj &operator=(const write_set_obj &right) = delete;  // NOLINT
  // for std::sort
  write_set_obj &operator=(write_set_obj &&right) = default;  // NOLINT

  bool operator<(const write_set_obj &right) const;  // NOLINT

  Record *get_rec_ptr() { return this->rec_ptr_; }  // NOLINT

  [[maybe_unused]] [[nodiscard]] const Record *get_rec_ptr() const {  // NOLINT
    return this->rec_ptr_;
  }

  /**
   * @brief get tuple ptr appropriately by operation type.
   * @return Tuple&
   */
  Tuple &get_tuple() { return get_tuple(op_); }  // NOLINT

  [[maybe_unused]] [[nodiscard]] const Tuple &get_tuple() const {  // NOLINT
    return get_tuple(op_);
  }

  /**
   * @brief get tuple ptr appropriately by operation type.
   * @return Tuple&
   */
  Tuple &get_tuple(const OP_TYPE op) {  // NOLINT
    if (op == OP_TYPE::UPDATE) {
      return get_tuple_to_local();
    }
    // insert/delete
    return get_tuple_to_db();
  }

  /**
   * @brief get tuple ptr appropriately by operation type.
   * @return const Tuple& const
   */
  [[nodiscard]] const Tuple &get_tuple(const OP_TYPE op) const {  // NOLINT
    if (op == OP_TYPE::UPDATE) {
      return get_tuple_to_local();
    }
    // insert/delete
    return get_tuple_to_db();
  }

  /**
   * @brief get tuple ptr to local write set
   * @return Tuple&
   */
  Tuple &get_tuple_to_local() { return this->tuple_; }  // NOLINT

  /**
   * @brief get tuple ptr to local write set
   * @return const Tuple&
   */
  [[nodiscard]] const Tuple &get_tuple_to_local() const {  // NOLINT
    return this->tuple_;
  }

  /**
   * @brief get tuple ptr to database(global)
   * @return Tuple&
   */
  Tuple &get_tuple_to_db() { return this->rec_ptr_->get_tuple(); }  // NOLINT

  /**
   * @brief get tuple ptr to database(global)
   * @return const Tuple&
   */
  [[nodiscard]] const Tuple &get_tuple_to_db() const {  // NOLINT
    return this->rec_ptr_->get_tuple();
  }

  OP_TYPE &get_op() { return op_; }  // NOLINT

  [[nodiscard]] const OP_TYPE &get_op() const { return op_; }  // NOLINT

  Storage get_st() const {
    return st_;
  }

  void reset_tuple_value(HeapObject&& obj) {
    get_tuple().set_value(std::move(obj));
  }

private:
  /**
   * for update : ptr to existing record.
   * for insert : ptr to new existing record.
   */
  alignas(CACHE_LINE_SIZE)
  OP_TYPE op_;
  Storage st_;
  Record *rec_ptr_;  // ptr to database
  Tuple tuple_;      // for update
};

class read_set_obj {  // NOLINT
public:
  read_set_obj() : rec_ptr(nullptr), is_scan(), st_() {
  }

  explicit read_set_obj(Storage st, const Record *rec_ptr, bool scan = false)  // NOLINT
    : rec_ptr(rec_ptr), is_scan{scan}, st_(st) {
  }

  read_set_obj(const read_set_obj &right) = delete;

  read_set_obj(read_set_obj &&right) = default;

  read_set_obj(read_set_obj &&right, bool scan) : is_scan{scan} {  // NOLINT
    rec_read = std::move(right.rec_read);
    rec_ptr = right.rec_ptr;
    st_ = right.st_;
  }

  read_set_obj &operator=(const read_set_obj &right) = delete;  // NOLINT
  read_set_obj &operator=(read_set_obj &&right) {               // NOLINT
    rec_read = std::move(right.rec_read);
    rec_ptr = right.rec_ptr;
    is_scan = right.is_scan; // required?
    st_ = right.st_;

    return *this;
  }

  [[nodiscard]] bool get_is_scan() const { return is_scan; }  // NOLINT

  Record &get_rec_read() { return rec_read; }  // NOLINT

  [[nodiscard]] const Record &get_rec_read() const {  // NOLINT
    return rec_read;
  }

  const Record *get_rec_ptr() { return rec_ptr; }  // NOLINT

  [[maybe_unused]] [[nodiscard]] const Record *get_rec_ptr() const {  // NOLINT
    return rec_ptr;
  }

  Storage get_st() const { return st_; }

private:
  alignas(CACHE_LINE_SIZE)
  Record rec_read{};
  const Record *rec_ptr{};  // ptr to database
  bool is_scan{false};      // NOLINT
  Storage st_;
};

// Operations for retry by abort
class opr_obj {  // NOLINT
public:
  opr_obj() = default;

  opr_obj(const OP_TYPE type, const char *key_ptr,            // NOLINT
          const std::size_t key_length)                       // NOLINT
          : type_(type), key_(key_ptr, key_length), value_() {}  // NOLINT
  opr_obj(const OP_TYPE type, const char *key_ptr,            // NOLINT
          const std::size_t key_length, const char *value_ptr,
          const std::size_t value_length)
          : type_(type),                        // NOLINT
            key_(key_ptr, key_length),          // NOLINT
            value_(value_ptr, value_length) {}  // NOLINT

  opr_obj(const opr_obj &right) = delete;

  opr_obj(opr_obj &&right) = default;

  opr_obj &operator=(const opr_obj &right) = delete;  // NOLINT
  opr_obj &operator=(opr_obj &&right) = default;      // NOLINT

  ~opr_obj() = default;

  OP_TYPE get_type() { return type_; }  // NOLINT
  std::string_view get_key() {          // NOLINT
    return {key_.data(), key_.size()};
  }

  std::string_view get_value() {  // NOLINT
    return {value_.data(), value_.size()};
  }

private:
  OP_TYPE type_{};
  std::string key_{};
  std::string value_{};
};

}  // namespace ccbench
