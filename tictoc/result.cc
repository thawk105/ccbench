
#include <iostream>

#include "include/common.hh"
#include "include/result.hh"

using std::cout;
using std::endl;
using std::fixed;
using std::setprecision;

#if ADD_ANALYSIS
void
TicTocResult::displayTotalTimestampHistorySuccessCounts()
{
  cout << "total_timestamp_history_success_counts:\t" << total_timestamp_history_success_counts_ << endl;
}

void
TicTocResult::displayTotalTimestampHistoryFailCounts()
{
  cout << "total_timestamp_history_fail_counts:\t" << total_timestamp_history_fail_counts_ << endl;
}

void
TicTocResult::displayTotalPreemptiveAbortsCounts()
{
  cout << "display_total_preemptive_aborts_counts:\t" << total_preemptive_aborts_counts_ << endl;
}

void
TicTocResult::displayTotalReadLatency()
{
  long double rate;
 if (total_read_latency_)
   rate = (long double)total_read_latency_ / ((long double)CLOCKS_PER_US * powl(10.0, 6.0) * (long double)EXTIME) / THREAD_NUM;
 else
   rate = 0;
 
 cout << fixed << setprecision(4) << "read_phase_rate:\t" << rate << endl;
}

void
TicTocResult::displayTotalValiLatency()
{
  long double rate;
 if (total_vali_latency_)
   rate = (long double)total_vali_latency_ / ((long double)CLOCKS_PER_US * powl(10.0, 6.0) * (long double)EXTIME) / THREAD_NUM;
 else
   rate = 0;
 
 cout << fixed << setprecision(4) << "validation_phase_rate:\t" << rate << endl;
}

void
TicTocResult::displayTotalExtraReads()
{
  cout << "total_extra_reads:\t" << total_extra_reads_ << endl;
}

void
TicTocResult::displayRtsupdRate()
{
  long double rate;
  if (total_rtsupd_)
    rate = (double)total_rtsupd_ / ((double)total_rtsupd_ + (double)total_rtsupd_chances_);
  else
    rate = 0;
  cout << fixed << setprecision(4) << "rtsupd_rate:\t" << rate << endl;
}

void
TicTocResult::displayRatioOfPreemptiveAbortToTotalAbort()
{
  long double pre_rate;
  if (total_preemptive_aborts_counts_)
    pre_rate = (double)total_preemptive_aborts_counts_ / (double)total_abort_counts_;
  else
    pre_rate = 0;
  cout << fixed << setprecision(4) << "ratio_of_preemptive_abort_to_total_abort:\t" << pre_rate << endl;
}

void
TicTocResult::addLocalTimestampHistorySuccessCounts(const uint64_t gcount)
{
  total_timestamp_history_success_counts_ += gcount;
}

void
TicTocResult::addLocalTimestampHistoryFailCounts(const uint64_t gcount)
{
  total_timestamp_history_fail_counts_ += gcount;
}

void
TicTocResult::addLocalPreemptiveAbortsCounts(const uint64_t acount)
{
  total_preemptive_aborts_counts_ += acount;
}

void
TicTocResult::addLocalRtsupdChances(const uint64_t rcount)
{
  total_rtsupd_chances_ += rcount;
}

void
TicTocResult::addLocalRtsupd(const uint64_t rcount)
{
  total_rtsupd_ += rcount;
}

void
TicTocResult::addLocalReadLatency(const uint64_t rcount)
{
  total_read_latency_ += rcount;
}

void
TicTocResult::addLocalValiLatency(const uint64_t vcount)
{
  total_vali_latency_ += vcount;
}

void
TicTocResult::addLocalExtraReads(const uint64_t ecount)
{
  total_extra_reads_ += ecount;
}
#endif

void
TicTocResult::addLocalAllTictocResult(const TicTocResult& other)
{
#if ADD_ANALYSIS
  addLocalExtraReads(other.local_extra_reads_);
  addLocalPreemptiveAbortsCounts(other.local_preemptive_aborts_counts_);
  addLocalRtsupdChances(other.local_rtsupd_chances_);
  addLocalRtsupd(other.local_rtsupd_);
  addLocalReadLatency(other.local_read_latency_);
  addLocalTimestampHistorySuccessCounts(other.local_timestamp_history_success_counts_);
  addLocalTimestampHistoryFailCounts(other.local_timestamp_history_fail_counts_);
  addLocalValiLatency(other.local_vali_latency_);
#endif
  addLocalAllResult(other);
}

void
TicTocResult::displayAllTictocResult()
{
#if ADD_ANALYSIS
  displayRtsupdRate();
  displayRatioOfPreemptiveAbortToTotalAbort();
  displayTotalExtraReads();
  displayTotalPreemptiveAbortsCounts();
  displayTotalReadLatency();
  displayTotalTimestampHistorySuccessCounts();
  displayTotalTimestampHistoryFailCounts();
  displayTotalValiLatency();
#endif
  displayAllResult();
}

