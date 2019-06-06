
#include <iomanip>
#include <iostream>

#include "include/common.hpp"
#include "include/result.hpp"

using std::cout;
using std::endl;
using std::fixed;
using std::setprecision;

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
CicadaResult::display_total_gc_tics()
{
  cout << "total_gc_tics:\t\t" << total_gc_tics << endl;
}

void
CicadaResult::display_gc_phase_rate()
{
  cout << fixed << setprecision(4) << "gc_phase_rate:\t\t" 
    << total_gc_tics / CLOCKS_PER_US / pow(10,6) / EXTIME / (THREAD_NUM - 1) << endl;
  /* (THREAD_NUM - 1) の理由。
   * leader thread がいるから。例として、3秒実験のうち、1秒がgc time だとしたら、1 * THREAD_NUM が
   * 算出されるのは意に沿わないため、ワーカー数で割る。 */
}

void
CicadaResult::display_AllCicadaResult()
{
  display_totalGCCounts();
  display_totalGCVersions();
  display_total_gc_tics();
  display_gc_phase_rate();
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
CicadaResult::add_local_gc_tics(uint64_t tics)
{
  total_gc_tics += tics;
}

void
CicadaResult::add_localAllCicadaResult(CicadaResult &other)
{
  add_localAllResult(other);
  add_localGCCounts(other.localGCCounts);
  add_localGCVersions(other.localGCVersions);
  add_local_gc_tics(other.local_gc_tics);
}

