#include "include/common.hpp"
#include "include/result.hpp"
#include <iomanip>
#include <iostream>

using std::cout, std::endl, std::fixed, std::setprecision;

void
MoccResult::display_total_abort_by_operation_rate()
{
  long double out;
  if (total_abort_by_operation)
    out = (long double)total_abort_by_operation / (long double)total_abort_counts;
  else
    out = 0;

  cout << "total_abort_by_operation:\t" << total_abort_by_operation << endl;
  cout << fixed << setprecision(6) << "total_abort_by_operation_rate:\t" << out << endl;
}

void
MoccResult::display_total_abort_by_validation_rate()
{
  long double out;
  if (total_abort_by_validation)
    out = (double)total_abort_by_validation / (double)total_abort_counts;
  else
    out = 0;

  cout << "total_abort_by_validation:\t" << total_abort_by_validation << endl;
  cout << fixed << setprecision(6) << "total_abort_by_validationRate:\t" << out << endl;
}

void
MoccResult::display_total_validation_failure_by_writelock_rate()
{
  long double out;
  if (total_validation_failure_by_writelock)
    out = (double)total_validation_failure_by_writelock / (double)total_abort_by_validation;
  else
    out = 0;

  cout << "total_validation_failure_by_writelock:\t" << total_validation_failure_by_writelock << endl;
  cout << fixed << setprecision(6) << "total_validation_failure_by_writelock_rate:\t" << out << endl;
}

void
MoccResult::display_total_validation_failure_by_tid_rate()
{
  long double out;
  if (total_validation_failure_by_tid)
    out = (double)total_validation_failure_by_tid / (double)total_abort_by_validation;
  else
    out = 0;

  cout << "total_validation_failure_by_tid:\t" << total_validation_failure_by_tid << endl;
  cout << fixed << setprecision(6) << "total_validation_failure_by_tidRate:\t\t" << out << endl;
}

void
MoccResult::display_total_temperature_resets()
{
  cout << "total_temperature_resets:\t" << total_temperature_resets << endl;
}

void
MoccResult::display_all_mocc_result()
{
  display_total_abort_by_operation_rate();
  display_total_abort_by_validation_rate();
  display_total_validation_failure_by_writelock_rate();
  display_total_validation_failure_by_tid_rate();
  display_total_temperature_resets();
  display_all_result();
}

void
MoccResult::add_local_abort_by_operation(uint64_t abo)
{
  total_abort_by_operation += abo;
}

void
MoccResult::add_local_abort_by_validation(uint64_t abv)
{
  total_abort_by_validation += abv;
}

void
MoccResult::add_local_validation_failure_by_writelock(uint64_t vfbwl)
{
  total_validation_failure_by_writelock += vfbwl;
}

void
MoccResult::add_local_validation_failure_by_tid(uint64_t vfbtid)
{
  total_validation_failure_by_tid += vfbtid;
}

void
MoccResult::add_local_temperature_resets(uint64_t tr)
{
  total_temperature_resets += tr;
}

void
MoccResult::add_local_all_mocc_result(MoccResult &other)
{
  add_local_all_result(other);
  add_local_abort_by_operation(other.local_abort_by_operation);
  add_local_abort_by_validation(other.local_abort_by_validation);
  add_local_validation_failure_by_writelock(other.local_validation_failure_by_writelock);
  add_local_validation_failure_by_tid(other.local_validation_failure_by_tid);
  add_local_temperature_resets(other.local_temperature_resets);
}

