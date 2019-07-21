
#include <iostream>

#include "include/result.hpp"

using std::cout, std::endl, std::fixed, std::setprecision;

void
SIResult::display_totalGCCounts()
{
  cout << "totalGCCounts:\t\t" << totalGCCounts << endl;
}

void
SIResult::display_totalGCVersions()
{
  cout << "totalGCVersions:\t" << totalGCVersions << endl;
}

void
SIResult::display_totalGCTMTElements()
{
  cout << "totalGCTMTElements:\t" << totalGCTMTElements << endl;
}

void
SIResult::display_AllSIResult()
{
#if ADD_ANALYSIS
  display_totalGCCounts();
  display_totalGCVersions();
  display_totalGCTMTElements();
#endif
  display_all_result();
}

void
SIResult::add_localGCCounts(const uint64_t gcount)
{
  totalGCCounts += gcount;
}

void
SIResult::add_localGCVersions(const uint64_t vcount)
{
  totalGCVersions += vcount;
}

void
SIResult::add_localGCTMTElements(const uint64_t ecount)
{
  totalGCTMTElements += ecount;
}

void
SIResult::add_localAllSIResult(const SIResult &other)
{
  add_local_all_result(other);
  add_localGCCounts(other.localGCCounts);
  add_localGCVersions(other.localGCVersions);
  add_localGCTMTElements(other.localGCTMTElements);
}

