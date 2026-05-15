
#include <new>

#include "../include/debug.hh"
#include "../include/util.hh"
#include "../include/zipf.hh"

size_t LENGTH;
double SKEW;
size_t TRIAL;

int main(const int argc, const char *argv[]) try {
  if (argc == 1) {
    cout << "./a.out LENGTH SKEW TRIAL" << endl;
    exit(0);
  }

  LENGTH = atoi(argv[1]);
  SKEW = atof(argv[2]);
  TRIAL = atoi(argv[3]);

  uint64_t Ctr[LENGTH];
//  if (posix_memalign((void**)&Ctr, CACHE_LINE_SIZE, LENGTH * sizeof(uint64_t)));
  Xoroshiro128Plus rnd;
  FastZipf zipf(&rnd, SKEW, LENGTH);

  for (size_t i = 0; i < LENGTH; ++i)
    Ctr[i] = 0;

  for (size_t i = 0; i < TRIAL; ++i)
    ++Ctr[zipf() % LENGTH];

  cout << "#number : count" << endl;
  for (size_t i = 0; i < LENGTH; ++i) {
    cout << i << " " << (double) Ctr[i] / (double) TRIAL << endl;
  }

  return 0;
} catch (std::bad_alloc) {
  ERR;
}
