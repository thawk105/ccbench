#include "include/common.hpp"
#include "include/result.hpp"
#include <iomanip>
#include <iostream>

using std::cout, std::endl, std::fixed, std::setprecision;

// forward declaration
extern uint64_t Result::Bgn, Result::End;

void 
Result::displayAbortCounts()
{
  cout << "Abort counts : " << AbortCounts << endl;
}

void
Result::displayAbortRate()
{
  long double ave_rate = (double)AbortCounts / (double)(CommitCounts + AbortCounts);
  cout << fixed << setprecision(4) << ave_rate << endl;
}

void
Result::displayTPS()
{
  uint64_t diff = End - Bgn;
  uint64_t sec = diff / CLOCK_PER_US / 1000 / 1000;

  uint64_t result = (double)CommitCounts / (double)sec;
  std::cout << (int)result << std::endl;
}
void
Result::sumUpAbortCounts()
{
  uint64_t expected, desired;
  expected = AbortCounts.load(std::memory_order_acquire);
  for (;;) {
    desired = expected + localAbortCounts;
    if (AbortCounts.compare_exchange_weak(expected, desired, std::memory_order_acq_rel, std::memory_order_acquire)) break;
  }
}

void
Result::sumUpCommitCounts()
{
  uint64_t expected, desired;
  expected = CommitCounts.load(std::memory_order_acquire);
  for (;;) {
    desired = expected + localCommitCounts;
    if (CommitCounts.compare_exchange_weak(expected, desired, std::memory_order_acq_rel, std::memory_order_acquire)) break;
  }
}

