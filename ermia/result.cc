
#include <iostream>

#include "include/result.hh"

using std::cout, std::endl, std::fixed, std::setprecision;

void
ErmiaResult::display_totalGCCounts()
{
  cout << "totalGCCounts:\t" << totalGCCounts << endl;
}

void
ErmiaResult::display_totalGCVersionCounts()
{
  cout << "totalGCVersionCounts:\t" << totalGCVersionCounts << endl;
}

void
ErmiaResult::display_totalGCTMTElementsCounts()
{
  cout << "totalGCTMTElementsCounts:\t" << totalGCTMTElementsCounts << endl;
}

void
ErmiaResult::display_AllErmiaResult()
{
#if ADD_ANALYSIS
  display_totalGCCounts();
  display_totalGCVersionCounts();
  display_totalGCTMTElementsCounts();
#endif
  displayAllResult();
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
  addLocalAllResult(other);
  add_localGCCounts(other.localGCCounts);
  add_localGCVersionCounts(other.localGCVersionCounts);
  add_localGCTMTElementsCounts(other.localGCTMTElementsCounts);
}

