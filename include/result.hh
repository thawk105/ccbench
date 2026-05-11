#pragma once

#include <atomic>
#include <iomanip>
#include <iostream>
#include <map>

#include "./cache_line_size.hh"

#define MAX_TX_TYPE 10

class Result {
public:
  alignas(CACHE_LINE_SIZE) uint64_t local_abort_counts_ = 0;
  alignas(CACHE_LINE_SIZE) uint64_t local_batch_abort_counts_ = 0;
  uint64_t local_commit_counts_ = 0;
  uint64_t local_batch_commit_counts_ = 0;
  uint64_t local_commit_counts_per_tx_[MAX_TX_TYPE] = {0};
  uint64_t local_abort_counts_per_tx_[MAX_TX_TYPE] = {0};
  int64_t local_latency_per_tx_[MAX_TX_TYPE] = {0};
  uint64_t local_success_fw_ = 0;
#if ADD_ANALYSIS
  uint64_t local_abort_by_operation_ = 0;
  uint64_t local_abort_by_validation_ = 0;
  uint64_t local_abort_latency_ = 0;
  uint64_t local_commit_latency_ = 0;
  uint64_t local_backoff_latency_ = 0;
  uint64_t local_early_aborts_ = 0;
  uint64_t local_extra_reads_ = 0;
  uint64_t local_gc_counts_ = 0;
  uint64_t local_gc_latency_ = 0;
  uint64_t local_gc_version_counts_ = 0;
  uint64_t local_gc_TMT_elements_counts_ = 0;
  uint64_t local_make_procedure_latency_ = 0;
  uint64_t local_memcpys = 0;
  uint64_t local_preemptive_aborts_counts_ = 0;
  uint64_t local_read_latency_ = 0;
  uint64_t local_rtsupd_ = 0;
  uint64_t local_rtsupd_chances_ = 0;
  uint64_t local_temperature_resets_ = 0;
  uint64_t local_timestamp_history_fail_counts_ = 0;
  uint64_t local_timestamp_history_success_counts_ = 0;
  uint64_t local_TMT_element_malloc_ = 0;
  uint64_t local_TMT_element_reuse_ = 0;
  uint64_t local_tree_traversal_ = 0;
  uint64_t local_vali_latency_ = 0;
  uint64_t local_validation_failure_by_tid_ = 0;
  uint64_t local_validation_failure_by_writelock_ = 0;
  uint64_t local_version_malloc_ = 0;
  uint64_t local_version_reuse_ = 0;
  uint64_t local_write_latency_ = 0;
  // only for oze
  uint64_t local_read_validation_latency_ = 0;
  uint64_t local_write_validation_latency_ = 0;
  uint64_t local_propagate_pages_ = 0;
  uint64_t local_graph_size_ = 0;
  uint64_t local_cycle_check_count_ = 0;
  uint64_t local_forwarding1_count_ = 0;
  uint64_t local_forwarding2_count_ = 0;
#endif

  uint64_t total_abort_counts_ = 0;
  uint64_t total_batch_abort_counts_ = 0;
  uint64_t total_commit_counts_ = 0;
  uint64_t total_batch_commit_counts_ = 0;
  uint64_t total_commit_counts_per_tx_[MAX_TX_TYPE] = {0};
  uint64_t total_abort_counts_per_tx_[MAX_TX_TYPE] = {0};
  uint64_t total_latency_per_tx_[MAX_TX_TYPE] = {0};
  uint64_t total_success_fw_ = 0;
#if ADD_ANALYSIS
  uint64_t total_abort_by_operation_ = 0;
  uint64_t total_abort_by_validation_ = 0;
  uint64_t total_abort_latency_ = 0;
  uint64_t total_commit_latency_ = 0;
  uint64_t total_backoff_latency_ = 0;
  uint64_t total_early_aborts_ = 0;
  uint64_t total_extra_reads_ = 0;
  uint64_t total_gc_counts_ = 0;
  uint64_t total_gc_latency_ = 0;
  uint64_t total_gc_version_counts_ = 0;
  uint64_t total_gc_TMT_elements_counts_ = 0;
  uint64_t total_make_procedure_latency_ = 0;
  uint64_t total_memcpys = 0;
  uint64_t total_preemptive_aborts_counts_ = 0;
  uint64_t total_read_latency_ = 0;
  uint64_t total_rtsupd_ = 0;
  uint64_t total_rtsupd_chances_ = 0;
  uint64_t total_temperature_resets_ = 0;
  uint64_t total_timestamp_history_fail_counts_ = 0;
  uint64_t total_timestamp_history_success_counts_ = 0;
  uint64_t total_TMT_element_malloc_ = 0;
  uint64_t total_TMT_element_reuse_ = 0;
  uint64_t total_tree_traversal_ = 0;
  uint64_t total_vali_latency_ = 0;
  uint64_t total_validation_failure_by_tid_ = 0;
  uint64_t total_validation_failure_by_writelock_ = 0;
  uint64_t total_version_malloc_ = 0;
  uint64_t total_version_reuse_ = 0;
  uint64_t total_write_latency_ = 0;
  // not exist local version.
  uint64_t total_latency_ = 0;
  // only for oze
  uint64_t total_read_validation_latency_ = 0;
  uint64_t total_write_validation_latency_ = 0;
  uint64_t total_propagate_pages_ = 0;
  uint64_t total_graph_size_ = 0;
  uint64_t total_cycle_check_count_ = 0;
  uint64_t total_forwarding1_count_ = 0;
  uint64_t total_forwarding2_count_ = 0;
#endif

  void displayAbortCounts();

  void displayAbortRate();

  void displayCommitCounts();

  void displaySuccessForwarding();

  void displayTps(size_t extime, size_t thread_num);

  void displayOps(size_t extime, size_t op_num, size_t batch_op_num);

  void displayAllResult(size_t clocks_per_us, size_t extime, size_t thread_num);

  void displayAllResult(size_t clocks_per_us, size_t extime, size_t thread_num,
                        size_t op_num, size_t batch_op_num);

  void displayPerTxResult(std::map<uint32_t,std::string> tx_types);

  void displayOzeAnalysisResult(size_t clocks_per_us, size_t extime, size_t thread_num);

#if ADD_ANALYSIS
  void displayAbortByOperationRate();   // abort by operation rate;
  void displayAbortByValidationRate();  // abort by validation rate;
  void displayAbortLatencyRate(size_t clocks_per_us, size_t extime,
                               size_t thread_num);
  void displayCommitLatencyRate(size_t clocks_per_us, size_t extime,
                                 size_t thread_num);
  void displayBackoffLatencyRate(size_t clocks_per_us, size_t extime,
                                 size_t thread_num);
  void displayEarlyAbortRate();
  void displayExtraReads();
  void displayGCCounts();
  void displayGCLatencyRate(size_t clocks_per_us, size_t extime,
                            size_t thread_num);
  void displayGCTMTElementsCounts();
  void displayGCVersionCounts();
  void displayMakeProcedureLatencyRate(size_t clocks_per_us, size_t extime,
      size_t thread_num);
  void displayMemcpys();
  void displayOtherWorkLatencyRate(size_t clocks_per_us, size_t extime,
      size_t thread_num);
  void displayPreemptiveAbortsCounts();
  void displayRatioOfPreemptiveAbortToTotalAbort();
  void displayReadLatencyRate(size_t clocks_per_us, size_t extime,
                              size_t thread_num);
  void displayRtsupdRate();
  void displayTemperatureResets();
  void displayTimestampHistorySuccessCounts();
  void displayTimestampHistoryFailCounts();
  void displayTreeTraversal();
  void displayTMTElementMalloc();
  void displayTMTElementReuse();
  void displayWriteLatencyRate(size_t clocks_per_us, size_t extime,
                               size_t thread_num);
  void displayValiLatencyRate(size_t clocks_per_us, size_t extime,
                              size_t thread_num);
  void displayReadValidationRate(size_t clocks_per_us, size_t extime, size_t thread_num);
  void displayWriteValidationRate(size_t clocks_per_us, size_t extime, size_t thread_num);
  void displayValidationFailureByTidRate();
  void displayValidationFailureByWritelockRate();
  void displayVersionMalloc();
  void displayVersionReuse();
  // only for oze
  void displayPropagatePages();
  void displayGraphSize();
  void displayCycleCheckCount();
  void displayForwardingCount();
#endif

  void addLocalAllResult(const Result &other);
  void addLocalPerTxResult(const Result &other, std::map<uint32_t,std::string> tx_types);

  void addLocalAbortCounts(const uint64_t count);
  void addLocalBatchAbortCounts(const uint64_t count);

  void addLocalCommitCounts(const uint64_t count);
  void addLocalBatchCommitCounts(const uint64_t count);

  void addLocalSuccessForwarding(const uint64_t count);

#if ADD_ANALYSIS
  void addLocalAbortByOperation(const uint64_t count);
  void addLocalAbortByValidation(const uint64_t count);
  void addLocalAbortLatency(const uint64_t count);
  void addLocalCommitLatency(const uint64_t count);
  void addLocalBackoffLatency(const uint64_t count);
  void addLocalEarlyAborts(const uint64_t count);
  void addLocalExtraReads(const uint64_t count);
  void addLocalGCCounts(const uint64_t count);
  void addLocalGCLatency(const uint64_t count);
  void addLocalGCVersionCounts(const uint64_t count);
  void addLocalGCTMTElementsCounts(const uint64_t count);
  void addLocalMakeProcedureLatency(const uint64_t count);
  void addLocalMemcpys(const uint64_t count);
  void addLocalPreemptiveAbortsCounts(const uint64_t count);
  void addLocalReadLatency(const uint64_t count);
  void addLocalRtsupd(const uint64_t count);
  void addLocalRtsupdChances(const uint64_t count);
  void addLocalTimestampHistorySuccessCounts(const uint64_t count);
  void addLocalTimestampHistoryFailCounts(const uint64_t count);
  void addLocalTemperatureResets(uint64_t count);
  void addLocalTMTElementsMalloc(const uint64_t count);
  void addLocalTMTElementsReuse(const uint64_t count);
  void addLocalTreeTraversal(const uint64_t count);
  void addLocalWriteLatency(const uint64_t count);
  void addLocalValiLatency(const uint64_t count);
  void addLocalValidationFailureByTid(const uint64_t count);
  void addLocalValidationFailureByWritelock(const uint64_t count);
  void addLocalVersionMalloc(const uint64_t count);
  void addLocalVersionReuse(const uint64_t count);
  // only for oze
  void addLocalReadValidationLatency(const uint64_t count);
  void addLocalWriteValidationLatency(const uint64_t count);
  void addLocalPropagatePages(const uint64_t count);
  void addLocalGraphSize(const uint64_t count);
  void addLocalCycleCheckCount(const uint64_t count);
  void addLocalForwarding1Count(const uint64_t count);
  void addLocalForwarding2Count(const uint64_t count);
#endif
};
