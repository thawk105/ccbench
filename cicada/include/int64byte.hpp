#pragma once
#include <atomic>
#include <cstdint>

using namespace std;

class uint64_t_64byte {
public:
	uint64_t num;
	int8_t padding[56];
};

class uint64_t_64byte_atomic {
public:
	atomic<uint64_t> num;
	int8_t padding[56];
};
