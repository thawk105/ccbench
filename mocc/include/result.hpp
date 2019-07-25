#pragma once

#include <atomic>

#include "../../include/result.hpp"

class MoccResult : public Result {
public:
  alignas(64)
  uint64_t total_abort_by_operation_ = 0;
  uint64_t total_abort_by_validation_ = 0;
  uint64_t total_validation_failure_by_writelock_ = 0;
  uint64_t total_validation_failure_by_tid_ = 0;
  uint64_t total_temperature_resets_ = 0;

  uint64_t local_abort_by_operation_ = 0;
  uint64_t local_abort_by_validation_ = 0;
  uint64_t local_validation_failure_by_writelock_ = 0;
  uint64_t local_validation_failure_by_tid_ = 0;
  uint64_t local_temperature_resets_ = 0;

  void display_total_abort_by_operation_rate(); // abort by operation rate;
  void display_total_abort_by_validation_rate(); // abort by validation rate;
  void display_total_validation_failure_by_writelock_rate();
  void display_total_validation_failure_by_tid_rate();
  void display_total_temperature_resets();
  void display_all_mocc_result();

  void add_local_abort_by_operation(uint64_t abo);
  void add_local_abort_by_validation(uint64_t abv);
  void add_local_validation_failure_by_writelock(uint64_t vfbwl);
  void add_local_validation_failure_by_tid(uint64_t vfbtid);
  void add_local_temperature_resets(uint64_t tr);
  void add_local_all_mocc_result(MoccResult &other);
};

