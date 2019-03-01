#pragma once

#include <atomic>

class Result {
private:
  static std::atomic<uint64_t> AbortCounts;
  static std::atomic<uint64_t> CommitCounts;

#ifdef DEBUG
  static std::atomic<uint64_t> AbortByOperation;
  static std::atomic<uint64_t> AbortByValidation;
  static std::atomic<uint64_t> ValidationFailureByWriteLock;
  static std::atomic<uint64_t> ValidationFailureByTID;
#endif

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

#ifdef DEBUG
  uint32_t localAbortByOperation = 0;
  uint32_t localAbortByValidation = 0;
  uint32_t localValidationFailureByWriteLock = 0;
  uint32_t localValidationFailureByTID = 0;

  void displayAbortByOperationRate(); // abort by operation rate;
  void displayAbortByValidationRate(); // abort by validation rate;
  void displayValidationFailureByWriteLockRate();
  void displayValidationFailureByTIDRate();
  void sumUpAbortByOperation();
  void sumUpAbortByValidation();
  void sumUpValidationFailureByWriteLock();
  void sumUpValidationFailureByTID();
#endif // DEBUG
};

#ifdef GLOBAL_VALUE_DEFINE
  // declare in ermia.cc
std::atomic<uint64_t> Result::AbortCounts(0);
std::atomic<uint64_t> Result::CommitCounts(0);
std::atomic<bool> Result::Finish(false);
uint64_t Result::Bgn(0);
uint64_t Result::End(0);

#ifdef DEBUG
std::atomic<uint64_t> Result::AbortByOperation(0);
std::atomic<uint64_t> Result::AbortByValidation(0);
std::atomic<uint64_t> Result::ValidationFailureByWriteLock(0);
std::atomic<uint64_t> Result::ValidationFailureByTID(0);
#endif // DEBUG

#endif // GLOBAL_VALUE_DEFINE
