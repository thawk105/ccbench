#pragma once

#include <atomic>

#include "../../include/result.hpp"

class SIResult : public Result {
public:
  uint64_t totalGCCounts = 0;
  uint64_t localGCCounts = 0;
  uint64_t totalGCVersions = 0;
  uint64_t localGCVersions = 0;
  uint64_t totalGCTMTElements = 0;
  uint64_t localGCTMTElements = 0;

  void display_totalGCCounts();
  void display_totalGCVersions();
  void display_totalGCTMTElements();
  void display_AllSIResult(const uint64_t clocks_per_us);
  void add_localAllSIResult(const SIResult &other);
  void add_localGCCounts(const uint64_t gcount);
  void add_localGCVersions(const uint64_t vcount);
  void add_localGCTMTElements(const uint64_t ecount);
};

