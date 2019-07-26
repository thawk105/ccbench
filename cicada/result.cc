
#include <iomanip>
#include <iostream>

#include "include/common.hh"
#include "include/result.hh"

using std::cout;
using std::endl;
using std::fixed;
using std::setprecision;

#if ADD_ANALYSIS
void
CicadaResult::displayTotalGCCounts()
{
  cout << "total_gc_counts_:\t\t" << total_gc_counts_ << endl;
}

void
CicadaResult::displayTotalGCVersions()
{
  cout << "total_gc_versions_:\t" << total_gc_versions_ << endl;
}

void
CicadaResult::displayTotalGCTics()
{
  cout << "total_gc_tics_:\t\t" << total_gc_tics_ << endl;
}

void
CicadaResult::displayGCPhaseRate()
{
  cout << fixed << setprecision(4) << "gc_phase_rate:\t\t" 
    << total_gc_tics_ / CLOCKS_PER_US / pow(10,6) / EXTIME / (THREAD_NUM - 1) << endl;
  /* (THREAD_NUM - 1) の理由。
   * leader thread がいるから。例として、3秒実験のうち、1秒がgc time だとしたら、1 * THREAD_NUM が
   * 算出されるのは意に沿わないため、ワーカー数で割る。 */
}

void
CicadaResult::addLocalGCCounts(uint64_t gcount)
{
  total_gc_counts_ += gcount;
}

void
CicadaResult::addLocalGCVersions(uint64_t vcount)
{
  total_gc_versions_ += vcount;
}

void
CicadaResult::addLocalGCTics(uint64_t tics)
{
  total_gc_tics_ += tics;
}
#endif

void
CicadaResult::addLocalAllCicadaResult(CicadaResult &other)
{
#if ADD_ANALYSIS
  addLocalGCCounts(other.local_gc_counts_);
  addLocalGCVersions(other.local_gc_versions_);
  addLocalGCTics(other.local_gc_tics_);
#endif
  addLocalAllResult(other);
}

void
CicadaResult::displayAllCicadaResult()
{
#if ADD_ANALYSIS
  displayTotalGCCounts();
  displayTotalGCVersions();
  displayTotalGCTics();
  displayGCPhaseRate();
#endif
  displayAllResult();
}

