
#include <iostream>

#include "include/common.hpp"
#include "include/result.hpp"

using std::cout;
using std::endl;
using std::fixed;
using std::setprecision;

void
SiloResult::display_total_read_latency()
{
  long double rate;
 if (total_read_latency)
   rate = (long double)total_read_latency / ((long double)CLOCKS_PER_US * powl(10.0, 6.0) * (long double)EXTIME) / THREAD_NUM;
 else
   rate = 0;
 
 cout << fixed << setprecision(4) << "read_phase_rate:\t" << rate << endl;
}

void
SiloResult::display_total_vali_latency()
{
  long double rate;
 if (total_vali_latency)
   rate = (long double)total_vali_latency / ((long double)CLOCKS_PER_US * powl(10.0, 6.0) * (long double)EXTIME) / THREAD_NUM;
 else
   rate = 0;
 
 cout << fixed << setprecision(4) << "validation_phase_rate:\t" << rate << endl;
}

void
SiloResult::display_total_extra_reads()
{
  cout << "total_extra_reads:\t" << total_extra_reads << endl;
}

void
SiloResult::display_all_silo_result()
{
  display_total_extra_reads();
  display_total_read_latency();
  display_total_vali_latency();
  display_all_result();
}

void
SiloResult::add_local_read_latency(const uint64_t rcount)
{
  total_read_latency += rcount;
}

void
SiloResult::add_local_vali_latency(const uint64_t vcount)
{
  total_vali_latency += vcount;
}

void
SiloResult::add_local_extra_reads(const uint64_t ecount)
{
  total_extra_reads += ecount;
}

void
SiloResult::add_local_all_silo_result(const SiloResult& other)
{
  add_local_read_latency(other.local_read_latency);
  add_local_vali_latency(other.local_vali_latency);
  add_local_extra_reads(other.local_extra_reads);
  add_local_all_result(other);
}

