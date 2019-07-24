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
  cout << "total_abort_counts_:\t" << total_abort_counts_ << endl;
}

void
Result::display_abort_rate()
{
  long double ave_rate = (double)total_abort_counts_ / (double)(total_commit_counts_ + total_abort_counts_);
  cout << fixed << setprecision(4) << "abort_rate:\t\t" << ave_rate << endl;
}

void
Result::display_total_commit_counts()
{
  cout << "total_commit_counts_:\t" << total_commit_counts_ << endl;
}

void
Result::display_tps()
{
  uint64_t result = total_commit_counts_ / extime_;
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
  total_abort_counts_ += other.local_abort_counts_;
  total_commit_counts_ += other.local_commit_counts_;
#if ADD_ANALYSIS
  total_tree_traversal_ += other.local_tree_traversal_;
#endif
}

void
Result::add_local_abort_counts(const uint64_t acount)
{
  total_abort_counts_ += acount;
}

void
Result::add_local_commit_counts(const uint64_t ccount)
{
  total_commit_counts_ += ccount;
}

#if ADD_ANALYSIS
void
Result::display_tree_traversal()
{
  cout << "tree_traversal:\t" << total_tree_traversal_ << endl;
}

void
Result::add_local_tree_traversal(const uint64_t tcount)
{
  total_tree_traversal_ += tcount;
}
#endif
