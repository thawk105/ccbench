#pragma once

#include "../../include/cache_line_size.hh"

#include "tidt .h"
#include "tuple.hh"

namespace silo_ext {

class record {
public:
private:
  alignas(CACHE_LINE_SIZE) Tidword tid_word_;
  Tuple tuple_;
};
} // namespace silo_ext
