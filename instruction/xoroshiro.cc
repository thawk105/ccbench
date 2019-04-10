
#include <cstdint>
#include <iostream>

#include "../include/tsc.hpp"
#include "../include/util.hpp"
#include "../include/random.hpp"
 
#define CLOCKS_PER_US 2100
#define LOOP 1000000
#define EX_TIME 3

using std::cout;
using std::endl;

int
main()
{
  Xoroshiro128Plus rnd;
  rnd.init();
  
  uint64_t start, stop;
  start = rdtscp();
  for (uint32_t i = 0; i < UINT32_MAX; ++i)
    rnd.next();
  stop = rdtscp();

  cout << "xoroshiro128PlusBench[clocks] :\t" 
    << (double)(stop - start)/(double)UINT32_MAX << endl;

  return 0;
}
