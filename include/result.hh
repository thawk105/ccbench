#pragma once

#include <atomic>
#include <iomanip>
#include <iostream>

#include "./cache_line_size.hh"

class Result {
public:
  alignas(CACHE_LINE_SIZE)
  static std::atomic<bool> Finish_;
  size_t clocks_per_us_ = 0;
  size_t extime_ = 0;
  size_t thid_ = 0;
  size_t thnum_ = 0;

  Result(){}
  Result(const size_t clocks_per_us, const size_t extime, const size_t thid, const size_t thnum) : clocks_per_us_(clocks_per_us), extime_(extime), thid_(thid), thnum_(thnum) {}

  uint64_t local_abort_counts_ = 0;
  uint64_t local_commit_counts_ = 0;
#if ADD_ANALYSIS
  uint64_t local_abort_by_operation_ = 0;
  uint64_t local_abort_by_validation_ = 0;
  uint64_t local_extra_reads_ = 0;
  uint64_t local_gc_counts_ = 0;
  uint64_t local_gc_latency_ = 0;
  uint64_t local_gc_version_counts_ = 0;
  uint64_t local_gc_TMT_elements_counts_ = 0;
  uint64_t local_preemptive_aborts_counts_ = 0;
  uint64_t local_read_latency_ = 0;
  uint64_t local_rtsupd_ = 0;
  uint64_t local_rtsupd_chances_ = 0;
  uint64_t local_temperature_resets_ = 0;
  uint64_t local_timestamp_history_fail_counts_ = 0;
  uint64_t local_timestamp_history_success_counts_ = 0;
  uint64_t local_tree_traversal_ = 0;
  uint64_t local_vali_latency_ = 0;
  uint64_t local_validation_failure_by_tid_ = 0;
  uint64_t local_validation_failure_by_writelock_ = 0;
  uint64_t local_write_latency_ = 0;
#endif

  uint64_t total_abort_counts_ = 0;
  uint64_t total_commit_counts_ = 0;
#if ADD_ANALYSIS
  uint64_t total_abort_by_operation_ = 0;
  uint64_t total_abort_by_validation_ = 0;
  uint64_t total_extra_reads_ = 0;
  uint64_t total_gc_counts_ = 0;
  uint64_t total_gc_latency_ = 0;
  uint64_t total_gc_version_counts_ = 0;
  uint64_t total_gc_TMT_elements_counts_ = 0;
  uint64_t total_preemptive_aborts_counts_ = 0;
  uint64_t total_read_latency_ = 0;
  uint64_t total_rtsupd_ = 0;
  uint64_t total_rtsupd_chances_ = 0;
  uint64_t total_temperature_resets_ = 0;
  uint64_t total_timestamp_history_fail_counts_ = 0;
  uint64_t total_timestamp_history_success_counts_ = 0;
  uint64_t total_tree_traversal_ = 0;
  uint64_t total_vali_latency_ = 0;
  uint64_t total_validation_failure_by_tid_ = 0;
  uint64_t total_validation_failure_by_writelock_ = 0;
  uint64_t total_write_latency_ = 0;
  // not exist local version.
  uint64_t total_latency_ = 0;
#endif

  void displayAbortCounts();
  void displayAbortRate();
  void displayCommitCounts();
  void displayTps();
  void displayAllResult();
#if ADD_ANALYSIS
  void displayAbortByOperationRate(); // abort by operation rate;
  void displayAbortByValidationRate(); // abort by validation rate;
  void displayExtraReads();
  void displayGCCounts();
  void displayGCLatencyRate();
  void displayGCTMTElementsCounts();
  void displayGCVersionCounts();
  void displayPreemptiveAbortsCounts();
  void displayRatioOfPreemptiveAbortToTotalAbort();
  void displayReadLatencyRate();
  void displayRtsupdRate();
  void displayTemperatureResets();
  void displayTimestampHistorySuccessCounts();
  void displayTimestampHistoryFailCounts();
  void displayTreeTraversal();
  void displayWriteLatencyRate();
  void displayValiLatencyRate();
  void displayValidationFailureByTidRate();
  void displayValidationFailureByWritelockRate();
#endif

  void addLocalAllResult(const Result &other);
  void addLocalAbortCounts(const uint64_t count);
  void addLocalCommitCounts(const uint64_t count);
#if ADD_ANALYSIS
  void addLocalAbortByOperation(const uint64_t count);
  void addLocalAbortByValidation(const uint64_t count);
  void addLocalExtraReads(const uint64_t count);
  void addLocalGCCounts(const uint64_t count);
  void addLocalGCLatency(const uint64_t count);
  void addLocalGCVersionCounts(const uint64_t count);
  void addLocalGCTMTElementsCounts(const uint64_t count);
  void addLocalPreemptiveAbortsCounts(const uint64_t count);
  void addLocalReadLatency(const uint64_t count);
  void addLocalRtsupd(const uint64_t count);
  void addLocalRtsupdChances(const uint64_t count);
  void addLocalTimestampHistorySuccessCounts(const uint64_t count);
  void addLocalTimestampHistoryFailCounts(const uint64_t count);
  void addLocalTemperatureResets(uint64_t count);
  void addLocalTreeTraversal(const uint64_t count);
  void addLocalWriteLatency(const uint64_t count);
  void addLocalValiLatency(const uint64_t count);
  void addLocalValidationFailureByTid(const uint64_t count);
  void addLocalValidationFailureByWritelock(const uint64_t count);
#endif

};

#ifdef GLOBAL_VALUE_DEFINE
std::atomic<bool> Result::Finish_(false);
#endif 
