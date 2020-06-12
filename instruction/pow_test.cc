
#include <cmath>
#include <chrono>
#include <iostream>

#include "../include/debug.hh"

#define TRIAL 1000000

using namespace std;

int main(const int argc, const char *argv[]) try {
  cout << "#pow_arg, time" << endl;
  for (double j = 0; j <= 3; j += 0.1) {
    chrono::system_clock::time_point start, end;
    start = chrono::system_clock::now();
    for (size_t i = 0; i < TRIAL; ++i)
      pow(2, j);
    end = chrono::system_clock::now();
    double time = static_cast<double>(chrono::duration_cast<chrono::microseconds>(end - start).count() / 1000.0);
    cout << j << " " << time << endl;
  }
  return 0;
} catch (std::bad_alloc) {
  ERR;
}
