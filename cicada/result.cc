
#include <iostream>

#include "include/result.hpp"

using std::cout, std::endl;

void
CicadaResult::display_totalGCCounts()
{
  cout << "totalGCCounts:\t\t" << totalGCCounts << endl;
}

void
CicadaResult::display_totalGCVersions()
{
  cout << "totalGCVersions:\t" << totalGCVersions << endl;
}

void
CicadaResult::display_AllCicadaResult()
{
  display_totalGCCounts();
  display_totalGCVersions();
  display_AllResult();
}

void
CicadaResult::add_localGCCounts(uint64_t gcount)
{
  totalGCCounts += gcount;
}

void
CicadaResult::add_localGCVersions(uint64_t vcount)
{
  totalGCVersions += vcount;
}

void
CicadaResult::add_localAllCicadaResult(CicadaResult &other)
{
  add_localAllResult(other);
  add_localGCCounts(other.localGCCounts);
  add_localGCVersions(other.localGCVersions);
}

