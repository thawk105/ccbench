
#include <iostream>

#include "include/result.hpp"

using std::cout, std::endl, std::fixed, std::setprecision;

void
ErmiaResult::display_totalGCCounts()
{
  cout << "totalGCCounts :\t\t" << totalGCCounts << endl;
}

void
ErmiaResult::display_totalGCVersionCounts()
{
  cout << "totalGC\nVersionCounts :\t\t" << totalGCVersionCounts << endl;
}

void
ErmiaResult::display_totalGCTMTElementsCounts()
{
  cout << "totalGC\nTMTElementsCounts :\t" << totalGCTMTElementsCounts << endl;
}

void
ErmiaResult::display_AllErmiaResult()
{
  display_totalGCCounts();
  display_totalGCVersionCounts();
  display_totalGCTMTElementsCounts();
  display_AllResult();
}

void
ErmiaResult::add_localGCCounts(const uint64_t gcount)
{
  totalGCCounts += gcount;
}

void
ErmiaResult::add_localGCVersionCounts(const uint64_t gcount)
{
  totalGCVersionCounts += gcount;
}

void
ErmiaResult::add_localGCTMTElementsCounts(const uint64_t gcount)
{
  totalGCTMTElementsCounts += gcount;
}

void
ErmiaResult::add_localAllErmiaResult(const ErmiaResult &other)
{
  add_localAllResult(other);
  add_localGCCounts(other.localGCCounts);
  add_localGCVersionCounts(other.localGCVersionCounts);
  add_localGCTMTElementsCounts(other.localGCTMTElementsCounts);
}

