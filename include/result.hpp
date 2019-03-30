#pragma once

#include <atomic>
#include <iomanip>
#include <iostream>

class Result {
public:
  static std::atomic<bool> Finish;
  uint64_t bgn = 0;
  uint64_t end = 0;
  uint64_t localAbortCounts = 0;
  uint64_t localCommitCounts = 0;
  uint64_t totalAbortCounts = 0;
  uint64_t totalCommitCounts = 0;
  uint32_t thid = 0;

  void display_totalAbortCounts();
  void display_abortRate();
  void display_totalCommitCounts();
  void display_tps(uint64_t clocks_per_us);
  void add_localAbortCounts(uint64_t acount);
  void add_localCommitCounts(uint64_t ccount);
};

#ifdef GLOBAL_VALUE_DEFINE
std::atomic<bool> Result::Finish(false);
#endif 
