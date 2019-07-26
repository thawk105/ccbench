
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

void 
Result::displayTotalAbortCounts()
{
  cout << "total_abort_counts_:\t" << total_abort_counts_ << endl;
}

void
Result::displayAbortRate()
{
  long double ave_rate = (double)total_abort_counts_ / (double)(total_commit_counts_ + total_abort_counts_);
  cout << fixed << setprecision(4) << "abort_rate:\t\t" << ave_rate << endl;
}

void
Result::displayTotalCommitCounts()
{
  cout << "total_commit_counts_:\t" << total_commit_counts_ << endl;
}

void
Result::displayTps()
{
  uint64_t result = total_commit_counts_ / extime_;
  cout << "Throughput(tps):\t" << result << endl;
}

void
Result::addLocalAbortCounts(const uint64_t acount)
{
  total_abort_counts_ += acount;
}

void
Result::addLocalCommitCounts(const uint64_t ccount)
{
  total_commit_counts_ += ccount;
}

#if ADD_ANALYSIS
void
Result::displayTotalGCCounts()
{
  cout << "total_gc_counts_:\t" << total_gc_counts_ << endl;
}

void
Result::displayTotalGCVersionCounts()
{
  cout << "total_gc_version_counts_:\t" << total_gc_version_counts_ << endl;
}

void
Result::displayTotalGCTMTElementsCounts()
{
  cout << "total_gc_TMT_elements_counts_:\t" << total_gc_TMT_elements_counts_ << endl;
}

void
Result::displayGCLatencyRate()
{
  long double rate;
  if (total_gc_latency_)
    rate = (long double)total_gc_latency_ / ((long double)clocks_per_us_ * powl(10.0, 6.0) * (long double)extime_) / thnum_;
  else
    rate = 0;

  cout << fixed << setprecision(4) << "gc_latency_rate:\t" << rate << endl;
}

void
Result::displayReadLatencyRate()
{
  long double rate;
  if (total_read_latency_)
    rate = (long double)total_read_latency_ / ((long double)clocks_per_us_ * powl(10.0, 6.0) * (long double)extime_) / thnum_;
  else
    rate = 0;

  cout << fixed << setprecision(4) << "raed_latency_rate:\t" << rate << endl;
}

void
Result::displayTreeTraversal()
{
  cout << "tree_traversal:\t" << total_tree_traversal_ << endl;
}

void
Result::displayWriteLatencyRate()
{
  long double rate;
  if (total_write_latency_)
    rate = (long double)total_write_latency_ / ((long double)clocks_per_us_ * powl(10.0, 6.0) * (long double)extime_) / thnum_;
  else
    rate = 0;

  cout << fixed << setprecision(4) << "write_latency_rate:\t" << rate << endl;
}

void
Result::addLocalGCCounts(const uint64_t gcount)
{
  total_gc_counts_ += gcount;
}

void
Result::addLocalGCVersionCounts(const uint64_t gcount)
{
  total_gc_version_counts_ += gcount;
}

void
Result::addLocalGCTMTElementsCounts(const uint64_t gcount)
{
  total_gc_TMT_elements_counts_ += gcount;
}

void
Result::addLocalGCLatency(const uint64_t gcount)
{
  total_gc_latency_ += gcount;
}

void
Result::addLocalReadLatency(const uint64_t rcount)
{
  total_read_latency_ += rcount;
}

void
Result::addLocalTreeTraversal(const uint64_t tcount)
{
  total_tree_traversal_ += tcount;
}

void
Result::addLocalWriteLatency(const uint64_t wcount)
{
  total_write_latency_ += wcount;
}
#endif

void
Result::displayAllResult()
{
#if ADD_ANALYSIS
  displayGCLatencyRate();
  displayTreeTraversal();
  displayReadLatencyRate();
  displayWriteLatencyRate();
#endif
  displayRusageRUMaxrss();
  displayTotalCommitCounts();
  displayTotalAbortCounts();
  displayAbortRate();
  displayTps();
}

void
Result::addLocalAllResult(const Result &other)
{
#if ADD_ANALYSIS
  total_gc_latency_ += other.local_gc_latency_;
  total_tree_traversal_ += other.local_tree_traversal_;
  total_read_latency_ += other.local_read_latency_;
  total_write_latency_ += other.local_write_latency_;
#endif
  total_abort_counts_ += other.local_abort_counts_;
  total_commit_counts_ += other.local_commit_counts_;
}

