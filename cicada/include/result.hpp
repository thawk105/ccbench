#pragma once

#include <atomic>

#include "../../include/result.hpp"

class CicadaResult : public Result {
public:
  uint64_t totalGCCounts = 0;
  uint64_t localGCCounts = 0;
  uint64_t totalGCVersions = 0;
  uint64_t localGCVersions = 0;

  void display_totalGCCounts();
  void display_totalGCVersions();
  void display_AllCicadaResult(const uint64_t clocks_per_us);
  void add_localAllCicadaResult(CicadaResult &other);
  void add_localGCCounts(uint64_t gcount);
  void add_localGCVersions(uint64_t vcount);
};

