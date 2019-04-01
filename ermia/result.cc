
#include <iostream>

#include "include/result.hpp"

using std::cout, std::endl, std::fixed, std::setprecision;

void
ErmiaResult::display_totalGCCounts()
{
  cout << "totalGCCounts :\t\t\t" << totalGCCounts << endl;
}

void
ErmiaResult::display_totalGCVersionCounts()
{
  cout << "totalGCVersionCounts :\t\t" << totalGCVersionCounts << endl;
}

void
ErmiaResult::display_totalGCTMTElementsCounts()
{
  cout << "totalGCTMTElementsCounts :\t" << totalGCTMTElementsCounts << endl;
}

void
ErmiaResult::add_localAll(ErmiaResult &other)
{
  totalAbortCounts += other.localAbortCounts;
  totalCommitCounts += other.localCommitCounts;
  totalGCCounts += other.localGCCounts;
  totalGCVersionCounts += other.localGCVersionCounts;
  totalGCTMTElementsCounts += other.localGCTMTElementsCounts;
}

void
ErmiaResult::add_localGCCounts(uint64_t gcount)
{
  totalGCCounts += gcount;
}

void
ErmiaResult::add_localGCVersionCounts(uint64_t gcount)
{
  totalGCVersionCounts += gcount;
}

void
ErmiaResult::add_localGCTMTElementsCounts(uint64_t gcount)
{
  totalGCTMTElementsCounts += gcount;
}

