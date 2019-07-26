#pragma once

#include <atomic>

#include "../../include/result.hh"

class TicTocResult : public Result {
public:
#if ADD_ANALYSIS
  uint64_t local_timestamp_history_success_counts_ = 0;
  uint64_t total_timestamp_history_success_counts_ = 0;
  uint64_t local_timestamp_history_fail_counts_ = 0;
  uint64_t total_timestamp_history_fail_counts_ = 0;
  uint64_t local_preemptive_aborts_counts_ = 0;
  uint64_t total_preemptive_aborts_counts_ = 0;
  uint64_t local_rtsupd_chances_ = 0;
  uint64_t local_rtsupd_ = 0;
  uint64_t total_rtsupd_chances_ = 0;
  uint64_t total_rtsupd_ = 0;
  uint64_t local_read_latency_ = 0; // readdation phase latency
  uint64_t total_read_latency_ = 0;
  uint64_t local_vali_latency_ = 0; // validation phase latency
  uint64_t total_vali_latency_ = 0;
  uint64_t local_extra_reads_ = 0;
  uint64_t total_extra_reads_ = 0;

  void displayRtsupdRate();
  void displayRatioOfPreemptiveAbortToTotalAbort();
  void displayTotalTimestampHistorySuccessCounts();
  void displayTotalTimestampHistoryFailCounts();
  void displayTotalPreemptiveAbortsCounts();
  void displayTotalExtraReads();
  void displayTotalReadLatency();
  void displayTotalValiLatency();
  void addLocalTimestampHistorySuccessCounts(const uint64_t gcount);
  void addLocalTimestampHistoryFailCounts(const uint64_t gcount);
  void addLocalPreemptiveAbortsCounts(const uint64_t acount);
  void addLocalRtsupdChances(const uint64_t rcount);
  void addLocalRtsupd(const uint64_t rcount);
  void addLocalReadLatency(const uint64_t rcount);
  void addLocalValiLatency(const uint64_t vcount);
  void addLocalExtraReads(const uint64_t ecount);
#endif
  void addLocalAllTictocResult(const TicTocResult& other);
  void displayAllTictocResult();
};

