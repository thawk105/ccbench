#pragma once

#include <atomic>

#include "../../include/result.hpp"

class TicTocResult : public Result {
public:
  uint64_t local_timestamp_history_success_counts = 0;
  uint64_t total_timestamp_history_success_counts = 0;
  uint64_t local_timestamp_history_fail_counts = 0;
  uint64_t total_timestamp_history_fail_counts = 0;

  void display_total_timestamp_history_success_counts();
  void display_total_timestamp_history_fail_counts();
  void display_all_tictoc_result();
  void add_local_timestamp_history_success_counts(const uint64_t gcount);
  void add_local_timestamp_history_fail_counts(const uint64_t gcount);
  void add_local_all_tictoc_result(const TicTocResult& other);
};

