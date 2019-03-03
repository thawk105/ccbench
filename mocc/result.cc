#include "include/common.hpp"
#include "include/result.hpp"
#include <iomanip>
#include <iostream>

using std::cout, std::endl, std::fixed, std::setprecision;

void 
Result::displayAbortCounts()
{
  cout << "Abort counts : " << AbortCounts << endl;
}

void
Result::displayAbortRate()
{
  long double ave_rate = (double)AbortCounts / (double)(CommitCounts + AbortCounts);
  cout << fixed << setprecision(4) << "AbortRate\t" << ave_rate << endl;
}

void
Result::displayTPS()
{
  uint64_t diff = End - Bgn;
  uint64_t sec = diff / CLOCK_PER_US / 1000 / 1000;

  uint64_t result = (double)CommitCounts / (double)sec;
  std::cout << "Throughput(tps)\t" << (int)result << std::endl;
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

#ifdef DEBUG
void
Result::displayAbortByOperationRate()
{
  long double out = (double)AbortByOperation / (double)AbortCounts;
  cout << "AbortByOperationRate\t\t" << out << endl;
}

void
Result::displayAbortByValidationRate()
{
  long double out = (double)AbortByValidation / (double)AbortCounts;
  cout << "AbortByValidationRate\t\t\t" << out << endl;
}

void
Result::displayValidationFailureByWriteLockRate()
{
  long double out = (double)ValidationFailureByWriteLock / (double)AbortByValidation;
  cout << "ValidationFailureByWriteLockRate\t" << out << endl;
}

void
Result::displayValidationFailureByTIDRate()
{
  long double out = (double)ValidationFailureByTID / (double)AbortByValidation;
  cout << "ValidationFailureByTIDRate\t\t" << out << endl;
}

void
Result::sumUpAbortByOperation()
{
  uint64_t expected, desired;
  expected = AbortByOperation.load(std::memory_order_acquire);
  for (;;) {
    desired = expected + localAbortByOperation;
    if (AbortByOperation.compare_exchange_weak(expected, desired, std::memory_order_acq_rel, std::memory_order_acquire)) break;
  }
}

void
Result::sumUpAbortByValidation()
{
  uint64_t expected, desired;
  expected = AbortByValidation.load(std::memory_order_acquire);
  for (;;) {
    desired = expected + localAbortByValidation;
    if (AbortByValidation.compare_exchange_weak(expected, desired, std::memory_order_acq_rel, std::memory_order_acquire)) break;
  }
}

void
Result::sumUpValidationFailureByWriteLock()
{
  uint64_t expected, desired;
  expected = ValidationFailureByWriteLock.load(std::memory_order_acquire);
  for (;;) {
    desired = expected + localValidationFailureByWriteLock;
    if (ValidationFailureByWriteLock.compare_exchange_weak(expected, desired, std::memory_order_acq_rel, std::memory_order_acquire)) break;
  }
}

void
Result::sumUpValidationFailureByTID()
{
  uint64_t expected, desired;
  expected = ValidationFailureByTID.load(std::memory_order_acquire);
  for (;;) {
    desired = expected + localValidationFailureByTID;
    if (ValidationFailureByTID.compare_exchange_weak(expected, desired, std::memory_order_acq_rel, std::memory_order_acquire)) break;
  }
}
#endif // DEBUG
