#include <iostream>
#include "include/random.hpp"
#include "include/zipf.hpp"

using std::cout, std::endl;

int
main()
{
  Xoroshiro128Plus rnd;
  rnd.init();

  FastZipf zipf(&rnd, 0.99999, 10);

  //for (uint i = 0; i < 10; ++i)
  //  cout << zipf() << endl;

  int ary[10] = {};
  for (uint i = 0; i < 10000; ++i)
    ++ary[zipf()];

  for (uint i = 0; i < 10; ++i)
    cout << "ary[" << i << "] = " << ary[i] << endl;

  return 0;
}
