#ifndef TUPLE_HPP
#define TUPLE_HPP

#include <atomic>
#include <cstdint>
#include <pthread.h>

class Tuple	{
public:
	std::atomic<uint64_t> tsword;
	unsigned int key = 0;
	std::atomic<unsigned int> val;
	int8_t padding[16];
};

class ReadElement {
public:
	uint64_t tsword;
	unsigned int key;
	unsigned int val;

	ReadElement(unsigned int key, unsigned int val, uint64_t tsword) {
		this->key = key;
		this->val = val;
		this->tsword = tsword;
	}

	bool operator<(const ReadElement& right) const {
		return this->key < right.key;
	}
};

class WriteElement {
public:
	unsigned int key, val;

	WriteElement(unsigned int key, unsigned int val) {
		this->key = key;
		this->val = val;
	}

	bool operator<(const WriteElement& right) const {
		return this->key < right.key;
	}
};

#endif	//	TUPLE_HPP
