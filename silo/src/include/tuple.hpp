#ifndef TUPLE_HPP
#define TUPLE_HPP

#include <atomic>
#include <cstdint>
#include <pthread.h>

class Tuple	{
public:
	std::atomic<uint64_t> tidword;
	unsigned int key = 0;
	std::atomic<unsigned int> val;
	int8_t padding[48];
};

#endif	//	TUPLE_HPP
