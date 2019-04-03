
#include <iostream>

#include "include/result.hpp"

using std::cout, std::endl, std::fixed, std::setprecision;

void
SIResult::display_totalGCCounts()
{
  cout << "totalGCCounts :\t\t" << totalGCCounts << endl;
}

void
SIResult::display_totalGCVersionCounts()
{
  cout << "totalGC\nVersionCounts :\t\t" << totalGCVersionCounts << endl;
}

void
SIResult::display_totalGCTMTElementsCounts()
{
  cout << "totalGC\nTMTElementsCounts :\t" << totalGCTMTElementsCounts << endl;
}

void
SIResult::display_AllSIResult(const uint64_t clocks_per_us)
{
  display_totalGCCounts();
  display_totalGCVersionCounts();
  display_totalGCTMTElementsCounts();
  display_AllResult(clocks_per_us);
}

void
SIResult::add_localGCCounts(const uint64_t gcount)
{
  totalGCCounts += gcount;
}

void
SIResult::add_localGCVersionCounts(const uint64_t gcount)
{
  totalGCVersionCounts += gcount;
}

void
SIResult::add_localGCTMTElementsCounts(const uint64_t gcount)
{
  totalGCTMTElementsCounts += gcount;
}

void
SIResult::add_localAllSIResult(const SIResult &other)
{
  add_localAllResult(other);
  add_localGCCounts(other.localGCCounts);
  add_localGCVersionCounts(other.localGCVersionCounts);
  add_localGCTMTElementsCounts(other.localGCTMTElementsCounts);
}

