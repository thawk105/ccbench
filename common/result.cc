#include <iomanip>
#include <iostream>

#include "../include/result.hpp"

using std::cout; 
using std::endl; 
using std::fixed; 
using std::setprecision;

extern void display_rusage_ru_maxrss();

void 
Result::display_total_abort_counts()
{
  cout << "total_abort_counts:\t" << total_abort_counts << endl;
}

void
Result::display_abort_rate()
{
  long double ave_rate = (double)total_abort_counts / (double)(total_commit_counts + total_abort_counts);
  cout << fixed << setprecision(4) << "abort_rate:\t\t" << ave_rate << endl;
}

void
Result::display_total_commit_counts()
{
  cout << "total_commit_counts:\t" << total_commit_counts << endl;
}

void
Result::display_tps()
{
  uint64_t result = total_commit_counts / extime;
  cout << "Throughput(tps):\t" << result << endl;
}

void
Result::display_all_result()
{
  display_rusage_ru_maxrss();
#if ADD_ANALYSIS
  display_tree_traversal();
#endif
  display_total_commit_counts();
  display_total_abort_counts();
  display_abort_rate();
  display_tps();
}

void
Result::add_local_all_result(const Result &other)
{
  total_abort_counts += other.local_abort_counts;
  total_commit_counts += other.local_commit_counts;
#if ADD_ANALYSIS
  total_tree_traversal += other.local_tree_traversal;
#endif
}

void
Result::add_local_abort_counts(const uint64_t acount)
{
  total_abort_counts += acount;
}

void
Result::add_local_commit_counts(const uint64_t ccount)
{
  total_commit_counts += ccount;
}

#if ADD_ANALYSIS
void
Result::display_tree_traversal()
{
  cout << "tree_traversal:\t" << total_tree_traversal << endl;
}

void
Result::add_local_tree_traversal(const uint64_t tcount)
{
  total_tree_traversal += tcount;
}
#endif
