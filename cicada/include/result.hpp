#pragma once

#include <atomic>

class Result {
private:
  static std::atomic<uint64_t> AbortCounts;
  static std::atomic<uint64_t> CommitCounts;
  static std::atomic<uint64_t> GCCounts;

public:
  static uint64_t Bgn;
  static uint64_t End;
  static std::atomic<bool> Finish;
  uint64_t localAbortCounts = 0;
  uint64_t localCommitCounts = 0;
  uint64_t localGCCounts = 0;
  unsigned int thid;

  Result() {}
  Result(unsigned int thid) {
    this->thid = thid;
  }

  void displayAbortCounts();
  void displayAbortRate();
  void displayCommitCounts();
  void displayGCCounts();
  void displayLocalCommitCounts();
  void displayTPS();
  void sumUpAbortCounts();
  void sumUpCommitCounts();
  void sumUpGCCounts();
};

#ifdef GLOBAL_VALUE_DEFINE
std::atomic<uint64_t> Result::AbortCounts(0);
std::atomic<uint64_t> Result::CommitCounts(0);
std::atomic<uint64_t> Result::GCCounts(0);
std::atomic<bool> Result::Finish(false);
uint64_t Result::Bgn(0);
uint64_t Result::End(0);
#endif 
