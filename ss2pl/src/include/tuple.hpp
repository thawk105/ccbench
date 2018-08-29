#ifndef TUPLE_HPP
#define TUPLE_HPP

#include <atomic>
#include <mutex>

#include "lock.hpp"

using namespace std;

class Tuple {
	public:
		RWLock lock;
		atomic<int> key;
		atomic<int> val;

		uint8_t padding[16];
};

#endif	//	TUPLE_HPP

