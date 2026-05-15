
#include <cstdint>
#include <iostream>

#include "../include/random.hh"
#include "../include/tsc.hh"
#include "../include/util.hh"

#define CLOCKS_PER_US 2100
#define LOOP 1000000
#define EX_TIME 3

using std::cout;
using std::endl;

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
