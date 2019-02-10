#pragma once

#include <atomic>

class Result {
private:
	static std::atomic<uint64_t> AbortCounts;
	static std::atomic<uint64_t> CommitCounts;
  static std::atomic<uint64_t> GCVersionCounts;
  static std::atomic<uint64_t> GCTMTElementsCounts;

public:
  static uint64_t Bgn;
  static uint64_t End;
	uint64_t localAbortCounts = 0;
	uint64_t localCommitCounts = 0;
  uint64_t localGCVersionCounts = 0;
  uint64_t localGCTMTElementsCounts = 0;


	void displayAbortCounts();
	void displayAbortRate();
  void displayGCVersionCountsPS(); // PS = per seconds;
  void displayGCTMTElementsCountsPS(); // PS = per seconds;
	void displayTPS();
	void sumUpAbortCounts();
	void sumUpCommitCounts();
  void sumUpGCVersionCounts();
  void sumUpGCTMTElementsCounts();
};

#ifdef GLOBAL_VALUE_DEFINE
	// declare in ermia.cc
std::atomic<uint64_t> Result::AbortCounts(0);
std::atomic<uint64_t> Result::CommitCounts(0);
std::atomic<uint64_t> Result::GCVersionCounts(0);
std::atomic<uint64_t> Result::GCTMTElementsCounts(0);
uint64_t Bgn(0);
uint64_t End(0);
#endif 
