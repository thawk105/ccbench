#pragma once

#include <atomic>
#include <iomanip>
#include <iostream>

class Result {
public:
  static std::atomic<bool> Finish;
  uint64_t bgn = 0;
  uint64_t end = 0;
  uint64_t thid = 0;
  size_t extime = 0;

  uint64_t local_abort_counts = 0;
  uint64_t local_commit_counts = 0;

  uint64_t total_abort_counts = 0;
  uint64_t total_commit_counts = 0;

  void display_total_abort_counts();
  void display_abort_rate();
  void display_total_commit_counts();
  void display_tps();
  void display_all_result();
  void add_local_all_result(const Result &other);
  void add_local_abort_counts(const uint64_t acount);
  void add_local_commit_counts(const uint64_t ccount);

#if ADD_ANALYSIS
  uint64_t local_tree_traversal = 0;
  uint64_t total_tree_traversal = 0;
  void display_tree_traversal();
  void add_local_tree_traversal(const uint64_t tcount);
#endif

};

#ifdef GLOBAL_VALUE_DEFINE
std::atomic<bool> Result::Finish(false);
#endif 
