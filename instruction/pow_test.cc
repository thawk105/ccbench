
#include <cmath>
#include <chrono>
#include <iostream>

#include "../include/debug.hh"

double POW_ARG;
#define TRIAL 1000000

using namespace std;

int main(const int argc, const char* argv[]) try {
  if (argc == 1) {
    cout << "./a.out POW_ARG" << endl;
  }

  POW_ARG = atof(argv[1]);

  chrono::system_clock::time_point start, end;
  start = chrono::system_clock::now();
  for (size_t i = 0; i < TRIAL; ++i)
    pow(2, POW_ARG);
  end = chrono::system_clock::now();
  double time = static_cast<double>(chrono::duration_cast<chrono::microseconds>(end - start).count() / 1000.0);
  cout << "execution time : " << time << " [ms]" << endl;
  return 0;
} catch (std::bad_alloc) {
  ERR;
}
