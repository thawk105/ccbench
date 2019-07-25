
#include <iostream>

#include "include/common.hh"
#include "include/result.hh"

using std::cout;
using std::endl;
using std::fixed;
using std::setprecision;

#if ADD_ANALYSIS
void
SiloResult::displayTotalReadLatency()
{
  long double rate;
 if (total_read_latency_)
   rate = (long double)total_read_latency_ / ((long double)CLOCKS_PER_US * powl(10.0, 6.0) * (long double)EXTIME) / THREAD_NUM;
 else
   rate = 0;
 
 cout << fixed << setprecision(4) << "read_phase_rate:\t" << rate << endl;
}

void
SiloResult::displayTotalValiLatency()
{
  long double rate;
 if (total_vali_latency_)
   rate = (long double)total_vali_latency_ / ((long double)CLOCKS_PER_US * powl(10.0, 6.0) * (long double)EXTIME) / THREAD_NUM;
 else
   rate = 0;
 
 cout << fixed << setprecision(4) << "validation_phase_rate:\t" << rate << endl;
}

void
SiloResult::displayTotalExtraReads()
{
  cout << "total_extra_reads:\t" << total_extra_reads_ << endl;
}

void
SiloResult::addLocalReadLatency(const uint64_t rcount)
{
  total_read_latency_ += rcount;
}

void
SiloResult::addLocalValiLatency(const uint64_t vcount)
{
  total_vali_latency_ += vcount;
}

void
SiloResult::addLocalExtraReads(const uint64_t ecount)
{
  total_extra_reads_ += ecount;
}
#endif

void
SiloResult::addLocalAllSiloResult(const SiloResult& other)
{
#if ADD_ANALYSIS
  addLocalReadLatency(other.local_read_latency_);
  addLocalValiLatency(other.local_vali_latency_);
  addLocalExtraReads(other.local_extra_reads_);
#endif
  addLocalAllResult(other);
}

void
SiloResult::displayAllSiloResult()
{
#if ADD_ANALYSIS
  displayTotalExtraReads();
  displayTotalReadLatency();
  displayTotalValiLatency();
#endif
  displayAllResult();
}

