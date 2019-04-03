#pragma once

#include <atomic>

#include "../../include/result.hpp"

class SIResult : public Result {
public:
  uint64_t totalGCCounts = 0;
  uint64_t localGCCounts = 0;
  uint64_t totalGCVersionCounts = 0;
  uint64_t localGCVersionCounts = 0;
  uint64_t totalGCTMTElementsCounts = 0;
  uint64_t localGCTMTElementsCounts = 0;

  void display_totalGCCounts();
  void display_totalGCVersionCounts();
  void display_totalGCTMTElementsCounts();
  void display_AllSIResult(const uint64_t clocks_per_us);
  void add_localAllSIResult(const SIResult &other);
  void add_localGCCounts(const uint64_t gcount);
  void add_localGCVersionCounts(const uint64_t gcount);
  void add_localGCTMTElementsCounts(const uint64_t gcount);
};

