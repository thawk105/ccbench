#pragma once

#include <atomic>

#include "../../include/result.hpp"

class MoccResult : public Result {
public:
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
  void display_AllMoccResult();

  void add_localAbortByOperation(uint64_t abo);
  void add_localAbortByValidation(uint64_t abv);
  void add_localValidationFailureByWriteLock(uint64_t vfbwl);
  void add_localValidationFailureByTID(uint64_t vfbtid);
  void add_localAllMoccResult(MoccResult &other);
};

