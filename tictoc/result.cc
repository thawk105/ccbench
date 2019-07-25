
#include <iostream>

#include "include/common.hpp"
#include "include/result.hpp"

using std::cout;
using std::endl;
using std::fixed;
using std::setprecision;

void
TicTocResult::display_total_timestamp_history_success_counts()
{
  cout << "total_timestamp_history_success_counts:\t" << total_timestamp_history_success_counts << endl;
}

void
TicTocResult::display_total_timestamp_history_fail_counts()
{
  cout << "total_timestamp_history_fail_counts:\t" << total_timestamp_history_fail_counts << endl;
}

void
TicTocResult::display_total_preemptive_aborts_counts()
{
  cout << "display_total_preemptive_aborts_counts:\t" << total_preemptive_aborts_counts << endl;
}

void
TicTocResult::display_total_read_latency()
{
  long double rate;
 if (total_read_latency)
   rate = (long double)total_read_latency / ((long double)CLOCKS_PER_US * powl(10.0, 6.0) * (long double)EXTIME) / THREAD_NUM;
 else
   rate = 0;
 
 cout << fixed << setprecision(4) << "read_phase_rate:\t" << rate << endl;
}

void
TicTocResult::display_total_vali_latency()
{
  long double rate;
 if (total_vali_latency)
   rate = (long double)total_vali_latency / ((long double)CLOCKS_PER_US * powl(10.0, 6.0) * (long double)EXTIME) / THREAD_NUM;
 else
   rate = 0;
 
 cout << fixed << setprecision(4) << "validation_phase_rate:\t" << rate << endl;
}

void
TicTocResult::display_total_extra_reads()
{
  cout << "total_extra_reads:\t" << total_extra_reads << endl;
}

void
TicTocResult::display_rtsupd_rate()
{
  long double rate;
  if (total_rtsupd)
    rate = (double)total_rtsupd / ((double)total_rtsupd + (double)total_rtsupd_chances);
  else
    rate = 0;
  cout << fixed << setprecision(4) << "rtsupd_rate:\t" << rate << endl;
}

void
TicTocResult::display_ratio_of_preemptive_abort_to_total_abort()
{
  long double pre_rate;
  if (total_preemptive_aborts_counts)
    pre_rate = (double)total_preemptive_aborts_counts / (double)total_abort_counts;
  else
    pre_rate = 0;
  cout << fixed << setprecision(4) << "ratio_of_preemptive_abort_to_total_abort:\t" << pre_rate << endl;
}

void
TicTocResult::display_all_tictoc_result()
{
  display_total_timestamp_history_success_counts();
  display_total_timestamp_history_fail_counts();
  display_total_preemptive_aborts_counts();
  display_ratio_of_preemptive_abort_to_total_abort();
  display_rtsupd_rate();
  display_total_read_latency();
  display_total_vali_latency();
  display_total_extra_reads();
  displayAllResult();
}

void
TicTocResult::add_local_timestamp_history_success_counts(const uint64_t gcount)
{
  total_timestamp_history_success_counts += gcount;
}

void
TicTocResult::add_local_timestamp_history_fail_counts(const uint64_t gcount)
{
  total_timestamp_history_fail_counts += gcount;
}

void
TicTocResult::add_local_preemptive_aborts_counts(const uint64_t acount)
{
  total_preemptive_aborts_counts += acount;
}

void
TicTocResult::add_local_rtsupd_chances(const uint64_t rcount)
{
  total_rtsupd_chances += rcount;
}

void
TicTocResult::add_local_rtsupd(const uint64_t rcount)
{
  total_rtsupd += rcount;
}

void
TicTocResult::add_local_read_latency(const uint64_t rcount)
{
  total_read_latency += rcount;
}

void
TicTocResult::add_local_vali_latency(const uint64_t vcount)
{
  total_vali_latency += vcount;
}

void
TicTocResult::add_local_extra_reads(const uint64_t ecount)
{
  total_extra_reads += ecount;
}

void
TicTocResult::add_local_all_tictoc_result(const TicTocResult& other)
{
  AddLocalAllResult(other);
  add_local_timestamp_history_success_counts(other.local_timestamp_history_success_counts);
  add_local_timestamp_history_fail_counts(other.local_timestamp_history_fail_counts);
  add_local_preemptive_aborts_counts(other.local_preemptive_aborts_counts);
  add_local_rtsupd_chances(other.local_rtsupd_chances);
  add_local_rtsupd(other.local_rtsupd);
  add_local_read_latency(other.local_read_latency);
  add_local_vali_latency(other.local_vali_latency);
  add_local_extra_reads(other.local_extra_reads);
}

