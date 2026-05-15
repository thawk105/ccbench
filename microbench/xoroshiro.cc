
#include <cstdint>
#include <iostream>

#include "../include/random.hh"
#include "../include/tsc.hh"

using std::cout;
using std::endl;

// Measures the average clock count of one Xoroshiro128Plus::next() call.
int main() {
  Xoroshiro128Plus rnd;
  rnd.init();

  uint64_t start, stop;
  start = rdtscp();
  for (uint32_t i = 0; i < UINT32_MAX; ++i) rnd.next();
  stop = rdtscp();

  cout << "xoroshiro128PlusBench[clocks] :\t"
       << (double) (stop - start) / (double) UINT32_MAX << endl;

  return 0;
}
