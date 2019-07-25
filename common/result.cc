#include <iomanip>
#include <iostream>

#include "../include/result.hh"

using std::cout; 
using std::endl; 
using std::fixed; 
using std::setprecision;

extern void displayRusageRUMaxrss();

void 
Result::displayTotalAbortCounts()
{
  cout << "total_abort_counts_:\t" << total_abort_counts_ << endl;
}

void
Result::displayAbortRate()
{
  long double ave_rate = (double)total_abort_counts_ / (double)(total_commit_counts_ + total_abort_counts_);
  cout << fixed << setprecision(4) << "abort_rate:\t\t" << ave_rate << endl;
}

void
Result::displayTotalCommitCounts()
{
  cout << "total_commit_counts_:\t" << total_commit_counts_ << endl;
}

void
Result::displayTps()
{
  uint64_t result = total_commit_counts_ / extime_;
  cout << "Throughput(tps):\t" << result << endl;
}

void
Result::displayAllResult()
{
  displayRusageRUMaxrss();
#if ADD_ANALYSIS
  displayTreeTraversal();
#endif
  displayTotalCommitCounts();
  displayTotalAbortCounts();
  displayAbortRate();
  displayTps();
}

void
Result::addLocalAllResult(const Result &other)
{
  total_abort_counts_ += other.local_abort_counts_;
  total_commit_counts_ += other.local_commit_counts_;
#if ADD_ANALYSIS
  total_tree_traversal_ += other.local_tree_traversal_;
#endif
}

void
Result::addLocalAbortCounts(const uint64_t acount)
{
  total_abort_counts_ += acount;
}

void
Result::addLocalCommitCounts(const uint64_t ccount)
{
  total_commit_counts_ += ccount;
}

#if ADD_ANALYSIS
void
Result::displayTreeTraversal()
{
  cout << "tree_traversal:\t" << total_tree_traversal_ << endl;
}

void
Result::addLocalTreeTraversal(const uint64_t tcount)
{
  total_tree_traversal_ += tcount;
}
#endif
