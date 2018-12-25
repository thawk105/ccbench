#pragma once

#include <atomic>
#include <cstdint>

#include "lock.hpp"

#define TEMP_THRESHOLD 5
#define TEMP_MAX 20
#define TEMP_RESET_US 100

struct Tidword {
	union {
		uint64_t obj;
		struct {
			bool lock:1;
			uint64_t tid:31;
			uint64_t epoch:32;
		};
	};

	Tidword() {
		obj = 0;
	}

	bool operator==(const Tidword& right) const {
		return obj == right.obj;
	}

	bool operator!=(const Tidword& right) const {
		return !operator==(right);
	}

	bool operator<(const Tidword& right) const {
		return this->obj < right.obj;
	}
};

// 32bit temprature, 32bit epoch
struct Epotemp {
	union {
		uint64_t obj;
		struct {
			uint64_t temp:32;
			uint64_t epoch:32;
		};
	};

	Epotemp() {
		obj = 0;
	}

	bool operator==(const Epotemp& right) const {
		return obj == right.obj;
	}

	bool operator!=(const Epotemp& right) const {
		return !operator==(right);
	}

	bool eqEpoch(uint64_t epo) {
		if (epoch == epo) return true;
		else return false;
	}
};

class Tuple	{
public:
	unsigned int key = 0;
	unsigned int val;
	Tidword tidword;
	Epotemp epotemp;	//	temprature, min 0, max 20
	MQLock mqlock;
	RWLock rwlock;	// 4byte
};

// use for read-write set
class ReadElement {
public:
	Tidword tidword;
	unsigned int key, val;
	bool failed_verification;

	ReadElement (Tidword tidword, unsigned int key, unsigned int val) {
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
