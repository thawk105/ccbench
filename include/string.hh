#pragma once

#include "debug.hh"
#include "fence.hh"

using std::cout;
using std::endl;

[[maybe_unused]] static void genStringRepeatedNumber(char *string, size_t val_size,
                                                     size_t thid) {
  size_t digit(1), thidnum(thid);
  for (;;) {
    thidnum /= 10;
    if (thidnum != 0)
      ++digit;
    else
      break;
  }

  // generate write value for this thread.
  sprintf(string, "%ld", thid);
  for (uint i = digit; i < val_size - 2; ++i) {
    string[i] = '0';
  }
  // printf("%s\n", string);
}
