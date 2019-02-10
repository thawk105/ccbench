#pragma once

#include <atomic>
#include <cstdint>

class uint64_t_64byte {
public:
	std::atomic<uint64_t> num;
	int8_t padding[56];
};
