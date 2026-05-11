
#include <cmath>
#include <iomanip>
#include <iostream>

#include "../include/debug.hh"
#include "../include/result.hh"

using std::cout;
using std::endl;
using std::fixed;
using std::setprecision;
using namespace std;

extern void displayRusageRUMaxrss();

void Result::displaySuccessForwarding() {
  cout << "success_forwarding_:\t" << total_success_fw_ << endl;
}

void Result::displayAbortCounts() {
  cout << "abort_counts_:\t" << total_abort_counts_ << endl;
  cout << "batch_abort_counts_:\t" << total_batch_abort_counts_ << endl;
}

void Result::displayAbortRate() {
  long double ave_rate = (double) total_abort_counts_ /
                         (double) (total_commit_counts_ + total_abort_counts_);
  cout << fixed << setprecision(4) << "abort_rate:\t" << ave_rate << endl;
  ave_rate = (double) total_batch_abort_counts_ /
             (double) (total_batch_commit_counts_ + total_batch_abort_counts_);
  cout << fixed << setprecision(4) << "batch_abort_rate:\t" << ave_rate << endl;
}

void Result::displayCommitCounts() {
  cout << "commit_counts_:\t" << total_commit_counts_ << endl;
  cout << "batch_commit_counts_:\t" << total_batch_commit_counts_ << endl;
}

void Result::displayTps(size_t extime, size_t thread_num) {
  uint64_t result = (total_commit_counts_ + total_batch_commit_counts_) / extime;
  cout << "latency[ns]:\t" << powl(10.0, 9.0) / result * thread_num << endl;
  cout << "throughput[tps]:\t" << result << endl;
}

void Result::displayOps(size_t extime, size_t op_num, size_t batch_op_num) {
  uint64_t result = (total_commit_counts_*op_num
                    + total_batch_commit_counts_*batch_op_num) / extime;
  cout << "throughput[ops]:\t" << result << endl;
}

#if ADD_ANALYSIS
void Result::displayAbortByOperationRate() {
  if (total_abort_by_operation_) {
    long double rate;
    rate = (long double)total_abort_by_operation_ /
           (long double)total_abort_counts_;
    cout << "abort_by_operation:\t" << total_abort_by_operation_ << endl;
    cout << fixed << setprecision(4) << "abort_by_operation_rate:\t" << rate
         << endl;
  }
}

void Result::displayAbortByValidationRate() {
  if (total_abort_by_validation_) {
    long double rate;
    rate = (double)total_abort_by_validation_ / (double)total_abort_counts_;
    cout << "abort_by_validation:\t" << total_abort_by_validation_ << endl;
    cout << fixed << setprecision(4) << "abort_by_validation_rate:\t" << rate
         << endl;
  }
}

void Result::displayAbortLatencyRate(size_t clocks_per_us, size_t extime,
                                      size_t thread_num) {
  if (total_abort_latency_) {
    long double rate;
    rate =
        (long double)total_abort_latency_ /
        ((long double)clocks_per_us * powl(10.0, 6.0) * (long double)extime) /
        thread_num;
    cout << fixed << setprecision(4) << "abort_latency_rate:\t" << rate
         << endl;
  }
}

void Result::displayCommitLatencyRate(size_t clocks_per_us, size_t extime,
                                      size_t thread_num) {
  if (total_commit_latency_) {
    long double rate;
    rate =
        (long double)total_commit_latency_ /
        ((long double)clocks_per_us * powl(10.0, 6.0) * (long double)extime) /
        thread_num;
    cout << fixed << setprecision(4) << "commit_latency_rate:\t" << rate
         << endl;
  }
}

void Result::displayBackoffLatencyRate(size_t clocks_per_us, size_t extime,
                                       size_t thread_num) {
  if (total_backoff_latency_) {
    long double rate;
    rate =
        (long double)total_backoff_latency_ /
        ((long double)clocks_per_us * powl(10.0, 6.0) * (long double)extime) /
        thread_num;
    cout << fixed << setprecision(4) << "backoff_latency_rate:\t" << rate
         << endl;
  }
}

void Result::displayEarlyAbortRate() {
  if (total_early_aborts_) {
    cout << fixed << setprecision(4) << "early_abort_rate:\t"
         << (long double)total_early_aborts_ / (long double)total_abort_counts_
         << endl;
  }
}

void Result::displayExtraReads() {
  cout << "extra_reads:\t" << total_extra_reads_ << endl;
}

void Result::displayGCCounts() {
  if (total_gc_counts_) cout << "gc_counts:\t" << total_gc_counts_ << endl;
}

void Result::displayGCLatencyRate(size_t clocks_per_us, size_t extime,
                                  size_t thread_num) {
  if (total_gc_latency_) {
    long double rate;
    rate =
        (long double)total_gc_latency_ /
        ((long double)clocks_per_us * powl(10.0, 6.0) * (long double)extime) /
        thread_num;
    cout << fixed << setprecision(4) << "gc_latency_rate:\t" << rate << endl;
  }
}

void Result::displayGCTMTElementsCounts() {
  if (total_gc_TMT_elements_counts_)
    cout << "gc_TMT_elements_counts:\t" << total_gc_TMT_elements_counts_
         << endl;
}

void Result::displayGCVersionCounts() {
  if (total_gc_version_counts_)
    cout << "gc_version_counts:\t" << total_gc_version_counts_ << endl;
}

void Result::displayMakeProcedureLatencyRate(size_t clocks_per_us,
                                             size_t extime, size_t thread_num) {
  if (total_make_procedure_latency_) {
    long double rate;
    rate =
        (long double)total_make_procedure_latency_ /
        ((long double)clocks_per_us * powl(10.0, 6.0) * (long double)extime) /
        thread_num;
    cout << fixed << setprecision(4) << "make_procedure_latency_rate:\t" << rate
         << endl;
  }
}

void Result::displayMemcpys() {
  if (total_memcpys) {
    cout << "memcpys:\t" << total_memcpys << endl;
  }
}

void Result::displayOtherWorkLatencyRate(size_t clocks_per_us, size_t extime,
                                         size_t thread_num) {
  long double sum_rate = 0;

  if (total_make_procedure_latency_) {
    sum_rate +=
        (long double)total_make_procedure_latency_ /
        ((long double)clocks_per_us * powl(10.0, 6.0) * (long double)extime) /
        thread_num;
  }
  if (total_read_latency_) {
    sum_rate +=
        (long double)total_read_latency_ /
        ((long double)clocks_per_us * powl(10.0, 6.0) * (long double)extime) /
        thread_num;
  }
  if (total_write_latency_) {
    sum_rate +=
        (long double)total_write_latency_ /
        ((long double)clocks_per_us * powl(10.0, 6.0) * (long double)extime) /
        thread_num;
  }
  if (total_vali_latency_) {
    sum_rate +=
        (long double)total_vali_latency_ /
        ((long double)clocks_per_us * powl(10.0, 6.0) * (long double)extime) /
        thread_num;
  }
  if (total_abort_latency_) {
    sum_rate +=
        (long double)total_abort_latency_ /
        ((long double)clocks_per_us * powl(10.0, 6.0) * (long double)extime) /
        thread_num;
  }
  if (total_gc_latency_) {
    sum_rate +=
        (long double)total_gc_latency_ /
        ((long double)clocks_per_us * powl(10.0, 6.0) * (long double)extime) /
        thread_num;
  }
  if (total_commit_latency_) {
    sum_rate +=
        (long double)total_commit_latency_ /
        ((long double)clocks_per_us * powl(10.0, 6.0) * (long double)extime) /
        thread_num;
  }

  cout << fixed << setprecision(4) << "other_work_latency_rate:\t"
       << (1.0 - sum_rate) << endl;
}

void Result::displayPreemptiveAbortsCounts() {
  if (total_preemptive_aborts_counts_)
    cout << "preemptive_aborts_counts:\t" << total_preemptive_aborts_counts_
         << endl;
}

void Result::displayRatioOfPreemptiveAbortToTotalAbort() {
  if (total_preemptive_aborts_counts_) {
    long double rate;
    rate =
        (double)total_preemptive_aborts_counts_ / (double)total_abort_counts_;
    cout << fixed << setprecision(4)
         << "ratio_of_preemptive_abort_to_total_abort:\t" << rate << endl;
  }
}

void Result::displayReadLatencyRate(size_t clocks_per_us, size_t extime,
                                    size_t thread_num) {
  if (total_read_latency_) {
    long double rate;
    rate =
        (long double)total_read_latency_ /
        ((long double)clocks_per_us * powl(10.0, 6.0) * (long double)extime) /
        thread_num;
    cout << fixed << setprecision(4) << "read_latency_rate:\t" << rate << endl;
  }
}

void Result::displayRtsupdRate() {
  if (total_rtsupd_chances_) {
    long double rate;
    rate = (double)total_rtsupd_ /
           ((double)total_rtsupd_ + (double)total_rtsupd_chances_);
    cout << fixed << setprecision(4) << "rtsupd_rate:\t" << rate << endl;
  }
}

void Result::displayTemperatureResets() {
  if (total_temperature_resets_)
    cout << "temperature_resets:\t" << total_temperature_resets_ << endl;
}

void Result::displayTimestampHistorySuccessCounts() {
  if (total_timestamp_history_success_counts_)
    cout << "timestamp_history_success_counts:\t"
         << total_timestamp_history_success_counts_ << endl;
}

void Result::displayTimestampHistoryFailCounts() {
  if (total_timestamp_history_fail_counts_)
    cout << "timestamp_history_fail_counts:\t"
         << total_timestamp_history_fail_counts_ << endl;
}

void Result::displayTreeTraversal() {
  if (total_tree_traversal_)
    cout << "tree_traversal:\t" << total_tree_traversal_ << endl;
}

void Result::displayTMTElementMalloc() {
  if (total_TMT_element_malloc_)
    cout << "TMT_element_malloc:\t" << total_TMT_element_malloc_ << endl;
}

void Result::displayTMTElementReuse() {
  if (total_TMT_element_reuse_)
    cout << "TMT_element_reuse:\t" << total_TMT_element_reuse_ << endl;
}

void Result::displayValiLatencyRate(size_t clocks_per_us, size_t extime,
                                    size_t thread_num) {
  if (total_vali_latency_) {
    long double rate;
    rate =
        (long double)total_vali_latency_ /
        ((long double)clocks_per_us * powl(10.0, 6.0) * (long double)extime) /
        thread_num;
    cout << fixed << setprecision(4) << "vali_latency_rate:\t" << rate << endl;
  }
}

void Result::displayValidationFailureByTidRate() {
  if (total_validation_failure_by_tid_) {
    long double rate;
    rate = (double)total_validation_failure_by_tid_ /
           (double)total_abort_by_validation_;
    cout << "validation_failure_by_tid:\t" << total_validation_failure_by_tid_
         << endl;
    cout << fixed << setprecision(4) << "validation_failure_by_tid_rate:\t"
         << rate << endl;
  }
}

void Result::displayValidationFailureByWritelockRate() {
  if (total_validation_failure_by_writelock_) {
    long double rate;
    rate = (double)total_validation_failure_by_writelock_ /
           (double)total_abort_by_validation_;
    cout << "validation_failure_by_writelock:\t"
         << total_validation_failure_by_writelock_ << endl;
    cout << fixed << setprecision(4)
         << "validation_failure_by_writelock_rate:\t" << rate << endl;
  }
}

void Result::displayVersionMalloc() {
  cout << "version_malloc:\t" << total_version_malloc_ << endl;
}

void Result::displayVersionReuse() {
  if (total_version_reuse_)
    cout << "version_reuse:\t" << total_version_reuse_ << endl;
}

void Result::displayWriteLatencyRate(size_t clocks_per_us, size_t extime,
                                     size_t thread_num) {
  if (total_write_latency_) {
    long double rate;
    rate =
        (long double)total_write_latency_ /
        ((long double)clocks_per_us * powl(10.0, 6.0) * (long double)extime) /
        thread_num;
    cout << fixed << setprecision(4) << "write_latency_rate:\t" << rate << endl;
  }
}

void Result::displayReadValidationRate(size_t clocks_per_us, size_t extime, size_t thread_num) {
  if (total_read_validation_latency_) {
    long double rate;
    rate =
        (long double)total_read_validation_latency_ /
        ((long double)clocks_per_us * powl(10.0, 6.0) * (long double)extime) /
        thread_num;
    cout << fixed << setprecision(4) << "read_validation_lat_rate:\t" << rate << endl;
  }
}

void Result::displayWriteValidationRate(size_t clocks_per_us, size_t extime, size_t thread_num) {
  if (total_write_validation_latency_) {
    long double rate;
    rate =
        (long double)total_write_validation_latency_ /
        ((long double)clocks_per_us * powl(10.0, 6.0) * (long double)extime) /
        thread_num;
    cout << fixed << setprecision(4) << "write_validation_lat_rate:\t" << rate << endl;
  }
}

void Result::displayPropagatePages() {
  uint64_t num_txns = total_commit_counts_ + total_abort_counts_;
  if (total_propagate_pages_) {
    long double n = (long double)total_propagate_pages_/(long double)num_txns;
    cout << fixed << setprecision(2) << "propagate_pages_per_tx: " << n << endl;
  }
}

void Result::displayGraphSize() {
  if (total_graph_size_ && total_cycle_check_count_) {
    long double n = (long double)total_graph_size_/(long double)total_cycle_check_count_;
    cout << fixed << setprecision(2) << "avg_graph_size_in_cycle_check: " << n << endl;
  }
}

void Result::displayCycleCheckCount() {
  uint64_t num_txns = total_commit_counts_ + total_abort_counts_;
  if (total_cycle_check_count_) {
    long double n = (long double)total_cycle_check_count_/(long double)num_txns;
    cout << fixed << setprecision(2) << "cycle_check_per_tx: " << n << endl;
  }
}

void Result::displayForwardingCount() {
  uint64_t num_txns = total_commit_counts_ + total_abort_counts_;
  auto n =
    total_forwarding1_count_; // / (long double)num_txns;
  cout << "1st_forwarding_count: " << n << endl;
  auto m =
    total_forwarding2_count_; // / (long double)num_txns;
  cout << "2nd_forwarding_count: " << m << endl;
}
#endif

void Result::addLocalAbortCounts(const uint64_t count) {
  total_abort_counts_ += count;
}

void Result::addLocalBatchAbortCounts(const uint64_t count) {
  total_batch_abort_counts_ += count;
}

void Result::addLocalCommitCounts(const uint64_t count) {
  total_commit_counts_ += count;
}

void Result::addLocalBatchCommitCounts(const uint64_t count) {
  total_batch_commit_counts_ += count;
}

void Result::addLocalSuccessForwarding(const uint64_t count) {
  total_success_fw_ += count;
}

#if ADD_ANALYSIS
void Result::addLocalAbortByOperation(const uint64_t count) {
  total_abort_by_operation_ += count;
}

void Result::addLocalAbortByValidation(const uint64_t count) {
  total_abort_by_validation_ += count;
}

void Result::addLocalAbortLatency(const uint64_t count) {
  total_abort_latency_ += count;
}

void Result::addLocalCommitLatency(const uint64_t count) {
  total_commit_latency_ += count;
}

void Result::addLocalBackoffLatency(const uint64_t count) {
  total_backoff_latency_ += count;
}

void Result::addLocalEarlyAborts(const uint64_t count) {
  total_early_aborts_ += count;
}

void Result::addLocalExtraReads(const uint64_t count) {
  total_extra_reads_ += count;
}

void Result::addLocalGCCounts(const uint64_t count) {
  total_gc_counts_ += count;
}

void Result::addLocalGCVersionCounts(const uint64_t count) {
  total_gc_version_counts_ += count;
}

void Result::addLocalGCTMTElementsCounts(const uint64_t count) {
  total_gc_TMT_elements_counts_ += count;
}

void Result::addLocalGCLatency(const uint64_t count) {
  total_gc_latency_ += count;
}

void Result::addLocalMakeProcedureLatency(const uint64_t count) {
  total_make_procedure_latency_ += count;
}

void Result::addLocalMemcpys(const uint64_t count) { total_memcpys += count; }

void Result::addLocalPreemptiveAbortsCounts(const uint64_t count) {
  total_preemptive_aborts_counts_ += count;
}

void Result::addLocalReadLatency(const uint64_t count) {
  total_read_latency_ += count;
}

void Result::addLocalRtsupd(const uint64_t count) { total_rtsupd_ += count; }

void Result::addLocalRtsupdChances(const uint64_t count) {
  total_rtsupd_chances_ += count;
}

void Result::addLocalTemperatureResets(const uint64_t count) {
  total_temperature_resets_ += count;
}

void Result::addLocalTMTElementsMalloc(const uint64_t count) {
  total_TMT_element_malloc_ += count;
}

void Result::addLocalTMTElementsReuse(const uint64_t count) {
  total_TMT_element_reuse_ += count;
}

void Result::addLocalTimestampHistoryFailCounts(const uint64_t count) {
  total_timestamp_history_fail_counts_ += count;
}

void Result::addLocalTimestampHistorySuccessCounts(const uint64_t count) {
  total_timestamp_history_success_counts_ += count;
}

void Result::addLocalTreeTraversal(const uint64_t count) {
  total_tree_traversal_ += count;
}

void Result::addLocalValiLatency(const uint64_t count) {
  total_vali_latency_ += count;
}

void Result::addLocalValidationFailureByTid(const uint64_t count) {
  total_validation_failure_by_tid_ += count;
}

void Result::addLocalValidationFailureByWritelock(const uint64_t count) {
  total_validation_failure_by_writelock_ += count;
}

void Result::addLocalVersionMalloc(const uint64_t count) {
  total_version_malloc_ += count;
}

void Result::addLocalVersionReuse(const uint64_t count) {
  total_version_reuse_ += count;
}

void Result::addLocalWriteLatency(const uint64_t count) {
  total_write_latency_ += count;
}

// only for oze
void Result::addLocalReadValidationLatency(const uint64_t count) { total_read_validation_latency_ += count; }
void Result::addLocalWriteValidationLatency(const uint64_t count) { total_write_validation_latency_ += count; }
void Result::addLocalPropagatePages(const uint64_t count) { total_propagate_pages_ += count; }
void Result::addLocalGraphSize(const uint64_t count) { total_graph_size_ += count; }
void Result::addLocalCycleCheckCount(const uint64_t count) { total_cycle_check_count_ += count; }
void Result::addLocalForwarding1Count(const uint64_t count) { total_forwarding1_count_ += count; }
void Result::addLocalForwarding2Count(const uint64_t count) { total_forwarding1_count_ += count; }
#endif

void Result::displayOzeAnalysisResult(size_t clocks_per_us, size_t extime, size_t thread_num) {
  // only for oze
#if ADD_ANALYSIS
  displayReadValidationRate(clocks_per_us, extime, thread_num);
  displayWriteValidationRate(clocks_per_us, extime, thread_num);
  displayPropagatePages();
  displayGraphSize();
  displayCycleCheckCount();
  // displayForwardingCount();
  // displaySuccessForwarding();
#endif
}

void Result::displayAllResult([[maybe_unused]] size_t clocks_per_us,
                              size_t extime,
                              [[maybe_unused]] size_t thread_num) {
#if ADD_ANALYSIS
  displayAbortByOperationRate();
  displayAbortByValidationRate();
  displayAbortLatencyRate(clocks_per_us, extime, thread_num);
  displayCommitLatencyRate(clocks_per_us, extime, thread_num);
  displayBackoffLatencyRate(clocks_per_us, extime, thread_num);
  displayEarlyAbortRate();
  displayExtraReads();
  displayGCCounts();
  displayGCLatencyRate(clocks_per_us, extime, thread_num);
  displayGCTMTElementsCounts();
  displayGCVersionCounts();
  displayMakeProcedureLatencyRate(clocks_per_us, extime, thread_num);
  displayMemcpys();
  displayOtherWorkLatencyRate(clocks_per_us, extime, thread_num);
  displayPreemptiveAbortsCounts();
  displayRatioOfPreemptiveAbortToTotalAbort();
  displayReadLatencyRate(clocks_per_us, extime, thread_num);
  displayRtsupdRate();
  displayTemperatureResets();
  displayTimestampHistorySuccessCounts();
  displayTimestampHistoryFailCounts();
  displayTMTElementMalloc();
  displayTMTElementReuse();
  displayTreeTraversal();
  displayWriteLatencyRate(clocks_per_us, extime, thread_num);
  displayValiLatencyRate(clocks_per_us, extime, thread_num);
  displayValidationFailureByTidRate();
  displayValidationFailureByWritelockRate();
  displayVersionMalloc();
  displayVersionReuse();
  displayOzeAnalysisResult(clocks_per_us, extime, thread_num);
#endif
  displaySuccessForwarding();
  displayAbortCounts();
  displayCommitCounts();
  displayRusageRUMaxrss();
  displayAbortRate();
  displayTps(extime, thread_num);
}

void Result::displayAllResult([[maybe_unused]] size_t clocks_per_us,
                              size_t extime,
                              [[maybe_unused]] size_t thread_num,
                              size_t op_num, size_t batch_op_num) {
#if ADD_ANALYSIS
  displayAbortByOperationRate();
  displayAbortByValidationRate();
  displayAbortLatencyRate(clocks_per_us, extime, thread_num);
  displayCommitLatencyRate(clocks_per_us, extime, thread_num);
  displayBackoffLatencyRate(clocks_per_us, extime, thread_num);
  displayEarlyAbortRate();
  displayExtraReads();
  displayGCCounts();
  displayGCLatencyRate(clocks_per_us, extime, thread_num);
  displayGCTMTElementsCounts();
  displayGCVersionCounts();
  displayMakeProcedureLatencyRate(clocks_per_us, extime, thread_num);
  displayMemcpys();
  displayOtherWorkLatencyRate(clocks_per_us, extime, thread_num);
  displayPreemptiveAbortsCounts();
  displayRatioOfPreemptiveAbortToTotalAbort();
  displayReadLatencyRate(clocks_per_us, extime, thread_num);
  displayRtsupdRate();
  displayTemperatureResets();
  displayTimestampHistorySuccessCounts();
  displayTimestampHistoryFailCounts();
  displayTMTElementMalloc();
  displayTMTElementReuse();
  displayTreeTraversal();
  displayWriteLatencyRate(clocks_per_us, extime, thread_num);
  displayValiLatencyRate(clocks_per_us, extime, thread_num);
  displayValidationFailureByTidRate();
  displayValidationFailureByWritelockRate();
  displayVersionMalloc();
  displayVersionReuse();
  displayOzeAnalysisResult(clocks_per_us, extime, thread_num);
#endif
  displayAbortCounts();
  displayCommitCounts();
  displayRusageRUMaxrss();
  displayAbortRate();
  displayTps(extime, thread_num);
  displayOps(extime, op_num, batch_op_num);
}

void Result::addLocalAllResult(const Result &other) {
  addLocalAbortCounts(other.local_abort_counts_);
  addLocalBatchAbortCounts(other.local_batch_abort_counts_);
  addLocalCommitCounts(other.local_commit_counts_);
  addLocalBatchCommitCounts(other.local_batch_commit_counts_);
  addLocalSuccessForwarding(other.local_success_fw_);
#if ADD_ANALYSIS
  addLocalAbortByOperation(other.local_abort_by_operation_);
  addLocalAbortByValidation(other.local_abort_by_validation_);
  addLocalAbortLatency(other.local_abort_latency_);
  addLocalBackoffLatency(other.local_backoff_latency_);
  addLocalCommitLatency(other.local_commit_latency_);
  addLocalEarlyAborts(other.local_early_aborts_);
  addLocalExtraReads(other.local_extra_reads_);
  addLocalGCCounts(other.local_gc_counts_);
  addLocalGCLatency(other.local_gc_latency_);
  addLocalGCVersionCounts(other.local_gc_version_counts_);
  addLocalGCTMTElementsCounts(other.local_gc_TMT_elements_counts_);
  addLocalMakeProcedureLatency(other.local_make_procedure_latency_);
  addLocalMemcpys(other.local_memcpys);
  addLocalPreemptiveAbortsCounts(other.local_preemptive_aborts_counts_);
  addLocalReadLatency(other.local_read_latency_);
  addLocalRtsupd(other.local_rtsupd_);
  addLocalRtsupdChances(other.local_rtsupd_chances_);
  addLocalTimestampHistorySuccessCounts(
      other.local_timestamp_history_success_counts_);
  addLocalTimestampHistoryFailCounts(
      other.local_timestamp_history_fail_counts_);
  addLocalTemperatureResets(other.local_temperature_resets_);
  addLocalTreeTraversal(other.local_tree_traversal_);
  addLocalTMTElementsMalloc(other.local_TMT_element_malloc_);
  addLocalTMTElementsReuse(other.local_TMT_element_reuse_);
  addLocalWriteLatency(other.local_write_latency_);
  addLocalValiLatency(other.local_vali_latency_);
  addLocalValidationFailureByTid(other.local_validation_failure_by_tid_);
  addLocalValidationFailureByWritelock(
      other.local_validation_failure_by_writelock_);
  addLocalVersionMalloc(other.local_version_malloc_);
  addLocalVersionReuse(other.local_version_reuse_);
  // only for oze
  addLocalReadValidationLatency(other.local_read_validation_latency_);
  addLocalWriteValidationLatency(other.local_write_validation_latency_);
  addLocalPropagatePages(other.local_propagate_pages_);
  addLocalGraphSize(other.local_graph_size_);
  addLocalCycleCheckCount(other.local_cycle_check_count_);
  addLocalForwarding1Count(other.local_forwarding1_count_);
  addLocalForwarding2Count(other.local_forwarding2_count_);
#endif
}

void Result::displayPerTxResult(std::map<uint32_t,std::string> tx_types) {
  for (auto& [type, name] : tx_types) {
    long double rate = (double) total_abort_counts_per_tx_[type] /
                       (double) (total_commit_counts_per_tx_[type] + total_abort_counts_per_tx_[type]);
    long double latency;
    if (total_commit_counts_per_tx_[type]) {
      latency = (double) total_latency_per_tx_[type] / (double) total_commit_counts_per_tx_[type];
    } else {
      latency = 0;
    }
    std::cout << "  Transaction type: " << name << std::endl;
    std::cout << "    commits: " << total_commit_counts_per_tx_[type] << std::endl;
    std::cout << "    aborts: " << total_abort_counts_per_tx_[type] << std::endl;
    std::cout << "    abort rate: "  << std::fixed << setprecision(4) << rate << std::endl;
    std::cout << "    latency[us]: " << std::fixed << setprecision(2) << latency << std::endl;
  }
  std::cout << "  Summary: ";
  for (auto& [type, name] : tx_types) {
    std::cout << total_commit_counts_per_tx_[type]
              << ","
              << total_abort_counts_per_tx_[type]
              << ",";
  }
  std::cout << std::endl;
  std::cout << "  All transaction total latencies: ";
  for (auto& [type, name] : tx_types) {
    std::cout << total_latency_per_tx_[type]
              << ",";
  }
  std::cout << std::endl;
}

void Result::addLocalPerTxResult(const Result &other, std::map<uint32_t,std::string> tx_types) {
  for (auto& [type, name] : tx_types) {
    total_commit_counts_per_tx_[type] += other.local_commit_counts_per_tx_[type];
    total_abort_counts_per_tx_[type] += other.local_abort_counts_per_tx_[type];
    total_latency_per_tx_[type] += other.local_latency_per_tx_[type];
  }
}