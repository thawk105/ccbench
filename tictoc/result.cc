
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
TicTocResult::display_all_tictoc_result()
{
  display_total_timestamp_history_success_counts();
  display_total_timestamp_history_fail_counts();
  display_all_result();
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
TicTocResult::add_local_all_tictoc_result(const TicTocResult& other)
{
  add_local_all_result(other);
  add_local_timestamp_history_success_counts(other.local_timestamp_history_success_counts);
  add_local_timestamp_history_fail_counts(other.local_timestamp_history_fail_counts);
}

