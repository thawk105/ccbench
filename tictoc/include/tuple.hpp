#pragma once

#include <atomic>
#include <cstdint>
#include <pthread.h>

struct TsWord {
	union {
		uint64_t obj;
		struct {
			bool lock:1;
			uint16_t delta:15;
			uint64_t wts:48;
		};
	};

	TsWord () {
		obj = 0;
	}

	bool operator==(const TsWord& right) const {
		return obj == right.obj;
	}

	bool operator!=(const TsWord& right) const {
		return !operator==(right);
	}

	bool isLocked() {
		if (lock) return true;
		else return false;
	}

	uint64_t rts() {
		return wts + delta;
	}

};
class Tuple	{
public:
	TsWord tsw;
	TsWord pre_tsw;
	// wts 48bit, rts-wts 15bit, lockbit 1bit
	unsigned int key = 0;
	unsigned int val;
	int8_t padding[8];
};

class SetElement {
public:
	unsigned int key;
	unsigned int val;
	TsWord tsw;

	SetElement(unsigned int key, unsigned int val, TsWord tsw) {
		this->key = key;
		this->val = val;
		this->tsw.obj = tsw.obj;
	}

	bool operator<(const SetElement& right) const {
		return this->key < right.key;
	}
};

class LockElement {
public:
	unsigned int key;

	LockElement(unsigned int key) {
		this->key = key;
	}
};
