#pragma once

#include <atomic>

#include "../../include/result.hh"

class SiloResult : public Result {
public:
#if ADD_ANALYSIS
  uint64_t local_read_latency_ = 0; // read phase latency
  uint64_t total_read_latency_ = 0;
  uint64_t local_vali_latency_ = 0; // validation phase latency
  uint64_t total_vali_latency_ = 0;
  uint64_t local_extra_reads_ = 0;
  uint64_t total_extra_reads_ = 0;

  void displayTotalExtraReads();
  void displayTotalValiLatency();
  void displayTotalReadLatency();
  void addLocalReadLatency(const uint64_t rcount);
  void addLocalValiLatency(const uint64_t vcount);
  void addLocalExtraReads(const uint64_t ecount);
#endif
  void displayAllSiloResult();
  void addLocalAllSiloResult(const SiloResult& other);
};
