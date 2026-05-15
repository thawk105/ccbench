
#include <cstdlib>
#include <new>
#include <vector>

#include "../include/debug.hh"
#include "../include/zipf.hh"

size_t LENGTH;
double SKEW;
size_t TRIAL;

// Dumps the histogram of FastZipf output to check the skew matches.
int main(const int argc, const char* argv[]) try {
  if (argc != 4) {
    cout << "./zipf_dist_test.exe LENGTH SKEW TRIAL" << endl;
    exit(0);
  }

  LENGTH = atoi(argv[1]);
  SKEW = atof(argv[2]);
  TRIAL = atoi(argv[3]);

  std::vector<uint64_t> Ctr(LENGTH, 0);
  Xoroshiro128Plus rnd;
  FastZipf zipf(&rnd, SKEW, LENGTH);

  for (size_t i = 0; i < TRIAL; ++i) ++Ctr[zipf() % LENGTH];

  cout << "#number : count" << endl;
  for (size_t i = 0; i < LENGTH; ++i) {
    cout << i << " " << (double) Ctr[i] / (double) TRIAL << endl;
  }

  return 0;
} catch (const std::bad_alloc&) { ERR; }
