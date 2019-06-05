
#include <iostream>

#include "include/result.hpp"

using std::cout;
using std::endl;
using std::fixed;
using std::setprecision;

void
TicTocResult::display_total_timestamp_history_success_counts()
{
  cout << "total_timestamp_history_success_counts:\t" << total_timestamp_history_success_counts << endl;
}

void
TicTocResult::display_total_timestamp_history_fail_counts()
{
  cout << "total_timestamp_history_fail_counts:\t" << total_timestamp_history_fail_counts << endl;
}

void
TicTocResult::display_all_TicTocResult()
{
  display_total_timestamp_history_success_counts();
  display_total_timestamp_history_fail_counts();
  display_AllResult();
}

void
TicTocResult::add_local_timestamp_history_success_counts(const uint64_t gcount)
{
  total_timestamp_history_success_counts += gcount;
}

void
TicTocResult::add_local_timestamp_history_fail_counts(const uint64_t gcount)
{
  total_timestamp_history_fail_counts += gcount;
}

void
TicTocResult::add_local_all_TicTocResult(const TicTocResult& other)
{
  add_localAllResult(other);
  add_local_timestamp_history_success_counts(other.local_timestamp_history_success_counts);
  add_local_timestamp_history_fail_counts(other.local_timestamp_history_fail_counts);
}

