#ifndef TUPLE_HPP
#define TUPLE_HPP
#include <atomic>
#include <cstdint>
#include "version.hpp"

class Tuple {
public: 
	std::atomic<Version *> latest;
	unsigned int key;
	std::atomic<signed char> lock;
	int8_t padding[51];
};

#endif	//	TUPLE_HPP
