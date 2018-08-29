#ifndef TUPLE_HPP
#define TUPLE_HPP
#include <atomic>
#include <cstdint>
#include "version.hpp"

class Tuple	{
public:
	std::atomic<int> gClock;
	int32_t key;
	std::atomic<Version *> latest;
	int8_t padding[48];		// 64

	Tuple() {
		latest.store(nullptr);
	}
};

#endif	//	TUPLE_HPP
