#ifndef TUPLE_HPP
#define TUPLE_HPP
#include <atomic>
#include <cstdint>
#include "version.hpp"

class Tuple	{
public:
	unsigned int key;
	std::atomic<Version *> latest;
	std::atomic<int> gClock;
	std::atomic<uint64_t> min_wts;
	int8_t padding[40];		// 64

	Tuple() {
		latest.store(nullptr);
	}
};

#endif	//	TUPLE_HPP
