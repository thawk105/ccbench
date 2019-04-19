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
Result::display_tps()
{
  uint64_t result = totalCommitCounts / extime;
  std::cout << "Throughput(tps):\t" << result << std::endl;
}

void
Result::display_AllResult()
{
  display_totalCommitCounts();
  display_totalAbortCounts();
  display_abortRate();
  display_tps();
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

