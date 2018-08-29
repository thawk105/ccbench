#ifndef INT64BYTE_HPP
#define INT64BYTE_HPP

#include <atomic>
#include <cstdint>

class uint64_t_64byte {
public:
	std::atomic<uint64_t> num;
	int8_t padding[56];
};

#endif	//	INT64BYTE
