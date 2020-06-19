#pragma once

#include <pthread.h>
#include <string.h>

#include <atomic>
#include <cstdint>

#include "../../include/cache_line_size.hh"

class Tuple {
 public:
  char val_[VAL_SIZE];
};
