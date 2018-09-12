#ifndef TUPLE_HPP
#define TUPLE_HPP

#include <atomic>
#include <cstdint>
#include <pthread.h>

class Tuple	{
public:
	std::atomic<uint64_t> tsword;
	// wts 48bit, rts-wts 15bit, lockbit 1bit
	unsigned int key = 0;
	std::atomic<unsigned int> val;
	int8_t padding[16];

	bool isLocked() {
		if (tsword & 1) return true;
		else return false;
	}
};

class SetElement {
public:
	unsigned int key;
	unsigned int val;
	uint64_t tsword;

	SetElement(unsigned int key, unsigned int val, uint64_t tsword) {
		this->key = key;
		this->val = val;
		this->tsword = tsword;
	}

	bool operator<(const SetElement& right) const {
		return this->key < right.key;
	}
};

#endif	//	TUPLE_HPP
