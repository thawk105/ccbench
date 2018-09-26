#pragma once

#include <atomic>
#include <cstdint>

#include "lock.hpp"

#define TEMP_THRESHOLD 5
#define TEMP_MAX 20
#define TEMP_RESET_US 100

using namespace std;

class Tuple	{
public:
	RWLock lock;	// 4byte
	unsigned int key = 0;
	atomic<uint64_t> tidword;
	atomic<uint64_t> temp;	//	temprature, min 0, max 20
	atomic<unsigned int> val;
	uint8_t padding[4];
};

// use for read-write set
class ReadElement {
public:
	uint64_t tidword;
	unsigned int key, val;
	bool failed_verification;

	ReadElement (uint64_t tidword, unsigned int key, unsigned int val) {
		this->tidword = tidword;
		this->key = key;
		this->val = val;
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
