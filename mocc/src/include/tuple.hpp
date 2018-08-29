#ifndef TUPLE_HPP
#define TUPLE_HPP

#include <atomic>
#include <cstdint>

#include "lock.hpp"

#define TEMP_THRESHOLD 10
#define TEMP_RESET_US 100

using namespace std;

class Tuple	{
public:
	RWLock lock;
	atomic<uint64_t> tidword;
	atomic<uint64_t> temp;	//	temprature, min 0, max 20
	unsigned int key = 0;
	atomic<unsigned int> val;
};

// use for read-write set
class ReadElement {
public:
	uint64_t tid;
	unsigned int key;
	bool failed_verification;

	ReadElement (uint64_t tid, unsigned int key) {
		this->tid = tid;
		this->key = key;
		this->failed_verification = false;
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
