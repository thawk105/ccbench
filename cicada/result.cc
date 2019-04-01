
#include <iostream>

#include "include/result.hpp"

using std::cout, std::endl;

void
CicadaResult::display_totalGCCounts()
{
  cout << "totalGCCounts :\t\t\t" << totalGCCounts << endl;
}

void
CicadaResult::add_localGCCounts(uint64_t gcount)
{
  totalGCCounts += gcount;
}

