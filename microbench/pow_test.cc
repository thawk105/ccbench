
#include <chrono>
#include <cmath>
#include <iostream>

#include "../include/debug.hh"

#define TRIAL 1000000

using namespace std;

// Measures the wall-clock time of std::pow(2, x) for x in [0, 3].
int main() try {
  cout << "#pow_arg, time" << endl;
  for (double j = 0; j <= 3; j += 0.1) {
    chrono::system_clock::time_point start, end;
    start = chrono::system_clock::now();
    // volatile sink so the loop body is not optimized away.
    volatile double sink = 0.0;
    for (size_t i = 0; i < TRIAL; ++i) sink = pow(2, j);
    (void) sink;
    end = chrono::system_clock::now();
    double time = static_cast<double>(
        chrono::duration_cast<chrono::microseconds>(end - start).count() /
        1000.0);
    cout << j << " " << time << endl;
  }
  return 0;
} catch (const std::bad_alloc &) {
  ERR;
}
