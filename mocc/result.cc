#include "include/common.hpp"
#include "include/result.hpp"
#include <iomanip>
#include <iostream>

using std::cout, std::endl, std::fixed, std::setprecision;

void
MoccResult::display_totalAbortByOperationRate()
{
  long double out = (double)totalAbortByOperation / (double)totalAbortCounts;
  cout << setprecision(6) << "totalAbort\nByOperationRate :\t" << out << endl;
}

void
MoccResult::display_totalAbortByValidationRate()
{
  long double out = (double)totalAbortByValidation / (double)totalAbortCounts;
  cout << setprecision(6) << "totalAbort\nByValidationRate :\t" << out << endl;
}

void
MoccResult::display_totalValidationFailureByWriteLockRate()
{
  long double out = (double)totalValidationFailureByWriteLock / (double)totalAbortByValidation;
  cout << setprecision(6) << "totalValidationFailure\nByWriteLockRate :\t" << out << endl;
}

void
MoccResult::display_totalValidationFailureByTIDRate()
{
  long double out = (double)totalValidationFailureByTID / (double)totalAbortByValidation;
  cout << setprecision(6) << "totalValidationFailure\nByTIDRate :\t\t" << out << endl;
}

void
MoccResult::display_AllMoccResult()
{
  display_totalAbortByOperationRate();
  display_totalAbortByValidationRate();
  display_totalValidationFailureByWriteLockRate();
  display_totalValidationFailureByTIDRate();
  display_AllResult();
}

void
MoccResult::add_localAbortByOperation(uint64_t abo)
{
  totalAbortByOperation += abo;
}

void
MoccResult::add_localAbortByValidation(uint64_t abv)
{
  totalAbortByValidation += abv;
}

void
MoccResult::add_localValidationFailureByWriteLock(uint64_t vfbwl)
{
  totalValidationFailureByWriteLock += vfbwl;
}

void
MoccResult::add_localValidationFailureByTID(uint64_t vfbtid)
{
  totalValidationFailureByTID += vfbtid;
}

void
MoccResult::add_localAllMoccResult(MoccResult &other)
{
  add_localAllResult(other);
  add_localAbortByOperation(other.localAbortByOperation);
  add_localAbortByValidation(other.localAbortByValidation);
  add_localValidationFailureByWriteLock(other.localValidationFailureByWriteLock);
  add_localValidationFailureByTID(other.localValidationFailureByTID);
}

