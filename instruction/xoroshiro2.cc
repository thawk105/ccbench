
#include <stdio.h>
#include <sys/time.h>

#include <iostream>

#include "../include/tsc.hpp"
#include "../include/random.hpp"

using namespace std;

int 
main()
{
  Xoroshiro128Plus rnd;
  rnd.init();

  uint64_t start, stop;

  //初回から数回は重いので，とりあえず100回ほど実行して肩を暖めます．
  for (int i = 0; i < 100; ++i) {
    rdtscp();
  }

  start = rdtscp();
  //gettimeofday(&bgn, NULL);
  for (int i = 0; i < 100000000; ++i) {
    rnd.next();
  }
  stop = rdtscp();
  //gettimeofday(&end, NULL);

  //auto timerval_calibration = ((end.tv_sec - bgn.tv_sec) + (end.tv_usec - bgn.tv_usec) * 0.000001) / (stop - start);
  cout << (stop - start) / 100000000.0 << endl;
  //fprintf(stderr, "timer/sec=%g[hz]\n", 1.0/timerval_calibration);
  //cout << 2.4*1000*1000*1000 / (stop-start) * 1000000 << endl;

  return 0;
}
