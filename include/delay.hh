#pragma once

#include <x86intrin.h>

#include <iostream>

#include "tsc.hh"

using namespace std;

[[maybe_unused]] static void
clock_delay(size_t clocks) {
  std::size_t start(rdtscp()), stop;

  for (;;) {
    stop = rdtscp();
    if (stop - start > clocks) {
      break;
    } else {
      _mm_pause();
    }
  }
}
