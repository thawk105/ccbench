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
  Record() {}

  Record(std::string_view key, std::string_view val, std::align_val_t align) : tuple_(key, val, align) {
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
  alignas(64)
  Tuple tuple_;
  tid_word tidw_;
};

}  // namespace ccbench
