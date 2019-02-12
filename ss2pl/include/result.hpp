#pragma once

#include <atomic>

class Result {
private:
  static std::atomic<uint64_t> AbortCounts;
  static std::atomic<uint64_t> CommitCounts;

public:
  static uint64_t Bgn;
  static uint64_t End;
  static std::atomic<bool> Finish;
  uint32_t localAbortCounts = 0;
  uint32_t localCommitCounts = 0;

  void displayAbortCounts();
  void displayAbortRate();
  void displayTPS();
  void sumUpAbortCounts();
  void sumUpCommitCounts();
};

#ifdef GLOBAL_VALUE_DEFINE
  // declare in ermia.cc
std::atomic<uint64_t> Result::AbortCounts(0);
std::atomic<uint64_t> Result::CommitCounts(0);
std::atomic<bool> Result::Finish(false);
uint64_t Bgn(0);
uint64_t End(0);
#endif 
