#pragma once

#include <atomic>

#include "../../include/result.hpp"

class SiloResult : public Result {
public:
  uint64_t local_read_latency = 0; // read phase latency
  uint64_t total_read_latency = 0;
  uint64_t local_vali_latency = 0; // validation phase latency
  uint64_t total_vali_latency = 0;
  uint64_t local_extra_reads = 0;
  uint64_t total_extra_reads = 0;

  void display_total_extra_reads();
  void display_total_vali_latency();
  void display_total_read_latency();
  void display_all_silo_result();
  void add_local_read_latency(const uint64_t rcount);
  void add_local_vali_latency(const uint64_t vcount);
  void add_local_extra_reads(const uint64_t ecount);
  void add_local_all_silo_result(const SiloResult& other);
};
