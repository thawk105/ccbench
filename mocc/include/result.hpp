#pragma once

#include <atomic>

#include "../../include/result.hpp"

class MoccResult : public Result {
  uint64_t totalAbortByOperation = 0;
  uint64_t totalAbortByValidation = 0;
  uint64_t totalValidationFailureByWriteLock = 0;
  uint64_t totalValidationFailureByTID = 0;

  uint64_t localAbortByOperation = 0;
  uint64_t localAbortByValidation = 0;
  uint64_t localValidationFailureByWriteLock = 0;
  uint64_t localValidationFailureByTID = 0;

  void display_totalAbortByOperationRate(); // abort by operation rate;
  void display_totalAbortByValidationRate(); // abort by validation rate;
  void display_totalValidationFailureByWriteLockRate();
  void display_totalValidationFailureByTIDRate();

  void add_localAbortByOperation(uint64_t abo);
};

#endif // GLOBAL_VALUE_DEFINE
