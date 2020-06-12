#include <iostream>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include "include/debug.hpp"
#include "include/random.hpp"
#include "include/tsc.hpp"
#include "include/zipf.hpp"

using std::cout, std::endl;

int
main() {
  pid_t pid = syscall(SYS_gettid);
  cpu_set_t cpu_set;

  CPU_ZERO(&cpu_set);
  CPU_SET(0, &cpu_set);

  if (sched_setaffinity(pid, sizeof(cpu_set_t), &cpu_set) != 0)
    ERR;

  Xoroshiro128Plus rnd;
  rnd.init();
  uint64_t start, stop, tmp;

  FastZipf zipf(&rnd, 0, 10);

  // warm up
  for (int i = 0; i < 100; ++i)
    start = rnd.next();

  start = rdtscp();
  for (int i = 0; i < 1000000; ++i)
    rnd.next();
  stop = rdtscp();

  cout << "xoroshiro : " << (stop - start) / 1000000 << endl;

  start = rdtscp();
  zipf();
  stop = rdtscp();

  cout << "zipf() : " << stop - start << endl;

  //for (uint i = 0; i < 10; ++i)
  //  cout << zipf() << endl;

  int ary[10] = {};
  for (uint i = 0; i < 10000; ++i)
    ++ary[zipf()];

  for (uint i = 0; i < 10; ++i)
    cout << "ary[" << i << "] = " << ary[i] << endl;

  return 0;
}
