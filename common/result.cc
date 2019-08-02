
#include <cmath>
#include <iomanip>
#include <iostream>

#include "../include/result.hh"

using std::cout;
using std::endl;
using std::fixed;
using std::setprecision;
using namespace std;

extern void displayRusageRUMaxrss();

void Result::displayAbortCounts() {
  cout << "abort_counts_:\t" << total_abort_counts_ << endl;
}

void Result::displayAbortRate() {
  long double ave_rate = (double)total_abort_counts_ /
                         (double)(total_commit_counts_ + total_abort_counts_);
  cout << fixed << setprecision(4) << "abort_rate:\t" << ave_rate << endl;
}

void Result::displayCommitCounts() {
  cout << "commit_counts_:\t" << total_commit_counts_ << endl;
}

void Result::displayTps() {
  uint64_t result = total_commit_counts_ / extime_;
  cout << "Throughput(tps):\t" << result << endl;
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

void Result::displayBackoffLatencyRate() {
  if (total_backoff_latency_) {
    long double rate;
    rate =
        (long double)total_backoff_latency_ /
        ((long double)clocks_per_us_ * powl(10.0, 6.0) * (long double)extime_) /
        thnum_;
    cout << fixed << setprecision(4) << "backoff_latency_rate:\t" << rate
         << endl;
  }
}

void Result::displayExtraReads() {
  if (total_extra_reads_)
    cout << "extra_reads:\t" << total_extra_reads_ << endl;
}

void Result::displayGCCounts() {
  if (total_gc_counts_) cout << "gc_counts:\t" << total_gc_counts_ << endl;
}

void Result::displayGCLatencyRate() {
  if (total_gc_latency_) {
    long double rate;
    rate =
        (long double)total_gc_latency_ /
        ((long double)clocks_per_us_ * powl(10.0, 6.0) * (long double)extime_) /
        thnum_;
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

void Result::displayReadLatencyRate() {
  if (total_read_latency_) {
    long double rate;
    rate =
        (long double)total_read_latency_ /
        ((long double)clocks_per_us_ * powl(10.0, 6.0) * (long double)extime_) /
        thnum_;
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

void Result::displayValiLatencyRate() {
  if (total_vali_latency_) {
    long double rate;
    rate =
        (long double)total_vali_latency_ /
        ((long double)clocks_per_us_ * powl(10.0, 6.0) * (long double)extime_) /
        thnum_;
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

void Result::displayWriteLatencyRate() {
  if (total_write_latency_) {
    long double rate;
    rate =
        (long double)total_write_latency_ /
        ((long double)clocks_per_us_ * powl(10.0, 6.0) * (long double)extime_) /
        thnum_;
    cout << fixed << setprecision(4) << "write_latency_rate:\t" << rate << endl;
  }
}
#endif
void Result::addLocalAbortCounts(const uint64_t count) {
  total_abort_counts_ += count;
}

void Result::addLocalCommitCounts(const uint64_t count) {
  total_commit_counts_ += count;
}

#if ADD_ANALYSIS
void Result::addLocalAbortByOperation(const uint64_t count) {
  total_abort_by_operation_ += count;
}

void Result::addLocalAbortByValidation(const uint64_t count) {
  total_abort_by_validation_ += count;
}

void Result::addLocalBackoffLatency(const uint64_t count) {
  total_backoff_latency_ += count;
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

void Result::addLocalValidationFailureByTid(uint64_t count) {
  total_validation_failure_by_tid_ += count;
}

void Result::addLocalValidationFailureByWritelock(uint64_t count) {
  total_validation_failure_by_writelock_ += count;
}

void Result::addLocalWriteLatency(const uint64_t count) {
  total_write_latency_ += count;
}
#endif

void Result::displayAllResult() {
#if ADD_ANALYSIS
  displayAbortByOperationRate();
  displayAbortByValidationRate();
  displayBackoffLatencyRate();
  displayExtraReads();
  displayGCCounts();
  displayGCLatencyRate();
  displayGCTMTElementsCounts();
  displayGCVersionCounts();
  displayPreemptiveAbortsCounts();
  displayRatioOfPreemptiveAbortToTotalAbort();
  displayReadLatencyRate();
  displayRtsupdRate();
  displayTemperatureResets();
  displayTimestampHistorySuccessCounts();
  displayTimestampHistoryFailCounts();
  displayTreeTraversal();
  displayWriteLatencyRate();
  displayValiLatencyRate();
  displayValidationFailureByTidRate();
  displayValidationFailureByWritelockRate();
#endif
  displayAbortCounts();
  displayCommitCounts();
  displayRusageRUMaxrss();
  displayAbortRate();
  displayTps();
}

void Result::addLocalAllResult(const Result &other) {
  addLocalAbortCounts(other.local_abort_counts_);
  addLocalCommitCounts(other.local_commit_counts_);
#if ADD_ANALYSIS
  addLocalAbortByOperation(other.local_abort_by_operation_);
  addLocalAbortByValidation(other.local_abort_by_validation_);
  addLocalBackoffLatency(other.local_backoff_latency_);
  addLocalExtraReads(other.local_extra_reads_);
  addLocalGCCounts(other.local_gc_counts_);
  addLocalGCLatency(other.local_gc_latency_);
  addLocalGCVersionCounts(other.local_gc_version_counts_);
  addLocalGCTMTElementsCounts(other.local_gc_TMT_elements_counts_);
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
  addLocalWriteLatency(other.local_write_latency_);
  addLocalValiLatency(other.local_vali_latency_);
  addLocalValidationFailureByTid(other.local_validation_failure_by_tid_);
  addLocalValidationFailureByWritelock(
      other.local_validation_failure_by_writelock_);
#endif
}
