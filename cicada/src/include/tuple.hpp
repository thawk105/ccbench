#ifndef TUPLE_HPP
#define TUPLE_HPP

#include <atomic>
#include <cstdint>

#include "version.hpp"

using namespace std;

class Tuple	{
public:
	atomic<Version *> latest;
	atomic<uint64_t> min_wts;
	unsigned int key;
	atomic<uint8_t> gClock;
	int8_t pad[3];
	int8_t pad2[40];

	Tuple() {
		latest.store(nullptr);
		gClock.store(0, memory_order_release);
	}
};

#endif	//	TUPLE_HPP
