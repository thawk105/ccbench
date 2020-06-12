#pragma once

#include <cstdint>

#include "./cache_line_size.hh"

class uint64_t_64byte {
public:
  alignas(CACHE_LINE_SIZE) uint64_t obj_;

  uint64_t_64byte() : obj_(0) {}

  uint64_t_64byte(uint64_t initial) : obj_(initial) {}
};
