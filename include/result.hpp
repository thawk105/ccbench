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
  size_t extime = 0;

  void display_totalAbortCounts();
  void display_abortRate();
  void display_totalCommitCounts();
  void display_tps();
  void display_AllResult();
  void add_localAllResult(const Result &other);
  void add_localAbortCounts(const uint64_t acount);
  void add_localCommitCounts(const uint64_t ccount);
};

#ifdef GLOBAL_VALUE_DEFINE
std::atomic<bool> Result::Finish(false);
#endif 
