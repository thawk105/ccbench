
#include <iostream>

#include "../include/tsc.hh"

#define LOOP 1000000

using std::cout;
using std::endl;

int main() {
  uint64_t start, stop;
  start = rdtscp();
  for (uint64_t i = 0; i < LOOP; ++i) rdtsc();
  stop = rdtscp();

  cout << "rdtscBench[clocks] :\t" << (stop - start) / LOOP << endl;

  start = rdtscp();
  for (unsigned int i = 0; i < LOOP; ++i) rdtscp();
  stop = rdtscp();

  cout << "rdtscpBench[clocks] :\t" << (stop - start) / LOOP << endl;
}
