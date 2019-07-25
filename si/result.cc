
#include <iostream>

#include "include/result.hh"

using std::cout, std::endl, std::fixed, std::setprecision;

void
SIResult::displayTotalGCCounts()
{
  cout << "total_gc_counts_:\t\t" << total_gc_counts_ << endl;
}

void
SIResult::displayTotalGCVersions()
{
  cout << "total_gc_versions_:\t" << total_gc_versions_ << endl;
}

void
SIResult::displayTotalGCTMTElements()
{
  cout << "total_gc_TMT_elements_:\t" << total_gc_TMT_elements_ << endl;
}

void
SIResult::displayAllSIResult()
{
#if ADD_ANALYSIS
  displayTotalGCCounts();
  displayTotalGCVersions();
  displayTotalGCTMTElements();
#endif
  displayAllResult();
}

void
SIResult::addLocalGCCounts(const uint64_t gcount)
{
  total_gc_counts_ += gcount;
}

void
SIResult::addLocalGCVersions(const uint64_t vcount)
{
  total_gc_versions_ += vcount;
}

void
SIResult::addLocalGCTMTElements(const uint64_t ecount)
{
  total_gc_TMT_elements_ += ecount;
}

void
SIResult::addLocalAllSIResult(const SIResult &other)
{
  addLocalAllResult(other);
  addLocalGCCounts(other.local_gc_counts_);
  addLocalGCVersions(other.local_gc_versions_);
  addLocalGCTMTElements(other.local_gc_TMT_elements_);
}

