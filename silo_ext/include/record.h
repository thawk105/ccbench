/**
 * @file record.h
 * @brief header about record
 */

#pragma once

#include "scheme_global.h"
#include "tuple.h"
#include "tid.h"

namespace ccbench {

class Record {  // NOLINT
public:
  Record() {}  // NOLINT

  Record(const char *key_ptr, const std::size_t key_length,
         const char *value_ptr, const std::size_t value_length)
          : tuple_(key_ptr, key_length, value_ptr, value_length) {
    // init tidw
    tidw_.set_absent(true);
    tidw_.set_lock(true);
  }

  Record(const Record &right) = default;

  Record(Record &&right) = default;

  Record &operator=(const Record &right) = default;  // NOLINT
  Record &operator=(Record &&right) = default;       // NOLINT

  tid_word &get_tidw() { return tidw_; }  // NOLINT

  [[nodiscard]] const tid_word &get_tidw() const { return tidw_; }  // NOLINT

  Tuple &get_tuple() { return tuple_; }  // NOLINT

  [[nodiscard]] const Tuple &get_tuple() const { return tuple_; }  // NOLINT

  void set_tidw(tid_word tidw) &{ tidw_.set_obj(tidw.get_obj()); }

private:
  tid_word tidw_;
  Tuple tuple_;
};

}  // namespace ccbench
