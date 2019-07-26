#pragma once

#include <atomic>
#include <iomanip>
#include <iostream>

#include "./cache_line_size.hh"

class Result {
public:
  alignas(CACHE_LINE_SIZE)
  static std::atomic<bool> Finish_;
  size_t thid_ = 0;
  size_t thnum_ = 0;
  size_t clocks_per_us_ = 0;
  size_t extime_ = 0;

  uint64_t local_abort_counts_ = 0;
  uint64_t local_commit_counts_ = 0;

  uint64_t total_abort_counts_ = 0;
  uint64_t total_commit_counts_ = 0;

  Result(){}
  Result(size_t thid, size_t thnum, size_t clocks_per_us, size_t extime) : thid_(thid), thnum_(thnum), clocks_per_us_(clocks_per_us), extime_(extime) {}

  void displayTotalAbortCounts();
  void displayAbortRate();
  void displayTotalCommitCounts();
  void displayTps();
  void displayAllResult();
  void addLocalAllResult(const Result &other);
  void addLocalAbortCounts(const uint64_t acount);
  void addLocalCommitCounts(const uint64_t ccount);

#if ADD_ANALYSIS
  uint64_t total_gc_counts_ = 0;
  uint64_t local_gc_counts_ = 0;
  uint64_t total_gc_version_counts_ = 0;
  uint64_t local_gc_version_counts_ = 0;
  uint64_t total_gc_TMT_elements_counts_ = 0;
  uint64_t local_gc_TMT_elements_counts_ = 0;
  uint64_t local_tree_traversal_ = 0;
  uint64_t total_tree_traversal_ = 0;
  uint64_t local_read_latency_ = 0;
  uint64_t total_read_latency_ = 0;
  uint64_t local_write_latency_ = 0;
  uint64_t total_write_latency_ = 0;
  uint64_t local_gc_latency_ = 0;
  uint64_t total_gc_latency_ = 0;
  uint64_t total_latency_ = 0;
  void displayGCLatencyRate();
  void displayReadLatencyRate();
  void displayTreeTraversal();
  void displayWriteLatencyRate();
  void displayTotalGCCounts();
  void displayTotalGCVersionCounts();
  void displayTotalGCTMTElementsCounts();
  void addLocalGCLatency(const uint64_t gcount);
  void addLocalReadLatency(const uint64_t rcount);
  void addLocalTreeTraversal(const uint64_t tcount);
  void addLocalWriteLatency(const uint64_t wcount);
  void addLocalGCCounts(const uint64_t gcount);
  void addLocalGCVersionCounts(const uint64_t gcount);
  void addLocalGCTMTElementsCounts(const uint64_t gcount);
#endif

};

#ifdef GLOBAL_VALUE_DEFINE
std::atomic<bool> Result::Finish_(false);
#endif 
