#include <iomanip>
#include <iostream>

#include "../include/result.hpp"

using std::cout, std::endl, std::fixed, std::setprecision;

void 
Result::display_totalAbortCounts()
{
  cout << "totalAbortCounts:\t" << totalAbortCounts << endl;
}

void
Result::display_abortRate()
{
  long double ave_rate = (double)totalAbortCounts / (double)(totalCommitCounts + totalAbortCounts);
  cout << fixed << setprecision(4) << "abortRate:\t\t" << ave_rate << endl;
}

void
Result::display_totalCommitCounts()
{
  cout << "totalCommitCounts:\t" << totalCommitCounts << endl;
}

void
Result::display_tps(const uint64_t clocks_per_us)
{
  uint64_t diff = end - bgn;
  uint64_t sec = diff / clocks_per_us / 1000 / 1000;

  uint64_t result = (double)totalCommitCounts / (double)sec;
  std::cout << "Throughput(tps):\t" << (int)result << std::endl;
}

void
Result::display_AllResult(const uint64_t clocks_per_us)
{
  display_totalCommitCounts();
  display_totalAbortCounts();
  display_abortRate();
  display_tps(clocks_per_us);
}

void
Result::add_localAllResult(const Result &other)
{
  totalAbortCounts += other.localAbortCounts;
  totalCommitCounts += other.localCommitCounts;
}

void
Result::add_localAbortCounts(const uint64_t acount)
{
  totalAbortCounts += acount;
}

void
Result::add_localCommitCounts(const uint64_t ccount)
{
  totalCommitCounts += ccount;
}

