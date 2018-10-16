#pragma once

#include <cstdint>

class uint64_t_64byte {
public:
	uint64_t obj;
	int8_t padding[56];

	uint64_t_64byte() { obj = 0; }
	uint64_t_64byte(uint64_t initial) {
		obj = initial;
	}
};
