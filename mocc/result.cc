#include "include/common.hh"
#include "include/result.hh"
#include <iomanip>
#include <iostream>

using std::cout, std::endl, std::fixed, std::setprecision;

void
MoccResult::display_total_abort_by_operation_rate()
{
  long double out;
  if (total_abort_by_operation_)
    out = (long double)total_abort_by_operation_ / (long double)total_abort_counts_;
  else
    out = 0;

  cout << "total_abort_by_operation:\t" << total_abort_by_operation_ << endl;
  cout << fixed << setprecision(6) << "total_abort_by_operation_rate:\t" << out << endl;
}

void
MoccResult::display_total_abort_by_validation_rate()
{
  long double out;
  if (total_abort_by_validation_)
    out = (double)total_abort_by_validation_ / (double)total_abort_counts_;
  else
    out = 0;

  cout << "total_abort_by_validation:\t" << total_abort_by_validation_ << endl;
  cout << fixed << setprecision(6) << "total_abort_by_validationRate:\t" << out << endl;
}

void
MoccResult::display_total_validation_failure_by_writelock_rate()
{
  long double out;
  if (total_validation_failure_by_writelock_)
    out = (double)total_validation_failure_by_writelock_ / (double)total_abort_by_validation_;
  else
    out = 0;

  cout << "total_validation_failure_by_writelock:\t" << total_validation_failure_by_writelock_ << endl;
  cout << fixed << setprecision(6) << "total_validation_failure_by_writelock_rate:\t" << out << endl;
}

void
MoccResult::display_total_validation_failure_by_tid_rate()
{
  long double out;
  if (total_validation_failure_by_tid_)
    out = (double)total_validation_failure_by_tid_ / (double)total_abort_by_validation_;
  else
    out = 0;

  cout << "total_validation_failure_by_tid:\t" << total_validation_failure_by_tid_ << endl;
  cout << fixed << setprecision(6) << "total_validation_failure_by_tidRate:\t\t" << out << endl;
}

void
MoccResult::display_total_temperature_resets()
{
  cout << "total_temperature_resets:\t" << total_temperature_resets_ << endl;
}

void
MoccResult::display_all_mocc_result()
{
#if ADD_ANALYSIS
  display_total_abort_by_operation_rate();
  display_total_abort_by_validation_rate();
  display_total_validation_failure_by_writelock_rate();
  display_total_validation_failure_by_tid_rate();
  display_total_temperature_resets();
#endif
  displayAllResult();
}

void
MoccResult::add_local_abort_by_operation(uint64_t abo)
{
  total_abort_by_operation_ += abo;
}

void
MoccResult::add_local_abort_by_validation(uint64_t abv)
{
  total_abort_by_validation_ += abv;
}

void
MoccResult::add_local_validation_failure_by_writelock(uint64_t vfbwl)
{
  total_validation_failure_by_writelock_ += vfbwl;
}

void
MoccResult::add_local_validation_failure_by_tid(uint64_t vfbtid)
{
  total_validation_failure_by_tid_ += vfbtid;
}

void
MoccResult::add_local_temperature_resets(uint64_t tr)
{
  total_temperature_resets_ += tr;
}

void
MoccResult::add_local_all_mocc_result(MoccResult &other)
{
  addLocalAllResult(other);
#if ADD_ANALYSIS
  add_local_abort_by_operation(other.local_abort_by_operation_);
  add_local_abort_by_validation(other.local_abort_by_validation_);
  add_local_validation_failure_by_writelock(other.local_validation_failure_by_writelock_);
  add_local_validation_failure_by_tid(other.local_validation_failure_by_tid_);
  add_local_temperature_resets(other.local_temperature_resets_);
#endif
}

