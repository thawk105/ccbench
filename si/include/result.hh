#pragma once

#include <atomic>

#include "../../include/result.hh"

class SIResult : public Result {
public:
  uint64_t total_gc_counts_ = 0;
  uint64_t local_gc_counts_ = 0;
  uint64_t total_gc_versions_ = 0;
  uint64_t local_gc_versions_ = 0;
  uint64_t total_gc_TMT_elements_ = 0;
  uint64_t local_gc_TMT_elements_ = 0;

  void displayTotalGCCounts();
  void displayTotalGCVersions();
  void displayTotalGCTMTElements();
  void displayAllSIResult();
  void addLocalAllSIResult(const SIResult &other);
  void addLocalGCCounts(const uint64_t gcount);
  void addLocalGCVersions(const uint64_t vcount);
  void addLocalGCTMTElements(const uint64_t ecount);
};

