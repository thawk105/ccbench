#include <iostream>
#include "include/tsc.hpp"
#include "include/random.hpp"

using namespace std;

int 
main()
{
	Xoroshiro128Plus rnd;
	rnd.init();

	uint64_t start, stop;

	for (int i = 0; i < 100; ++i) {
		rdtsc();
	}

	start = rdtsc();
	for (int i = 0; i < 1000000; ++i) {
		rnd.next();
	}
	stop = rdtsc();

	cout << (stop - start) / 1000000 << endl;

	return 0;
}
