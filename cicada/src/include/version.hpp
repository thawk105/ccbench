#pragma once

#include <atomic>
#include <cstdint>
#include <sys/time.h>
#include "timeStamp.hpp"

using namespace std;

enum class VersionStatus : uint8_t {
	invalid,
	pending,
	aborted,
	precommitted,	//early lock releaseで利用
	committed,
	deleted,
};

class Version {
public:
	atomic<uint64_t> rts;
	atomic<uint64_t> wts;
	
	unsigned int key;	//log what is this key.
	unsigned int val;			
	atomic<Version *> next;

	atomic<VersionStatus> status;	//commit record
	int8_t pad[7];
	int8_t pad2[24];

	Version() {
		status.store(VersionStatus::pending, memory_order_release);
		next.store(nullptr, memory_order_release);
	}

	Version(uint64_t rts, uint64_t wts, unsigned int key, unsigned int val) {
		this->rts.store(rts, memory_order_relaxed);
		this->wts.store(wts, memory_order_relaxed);
		this->key = key;
		this->val = val;
	}
};

class ElementSet {
public:
	Version *sourceObject = nullptr;
	Version *newObject = nullptr;
};

class ReadElement {
public:
	unsigned int key;
	Version *ver;

	ReadElement(unsigned int key, Version *ver) {
		this->key = key;
		this->ver = ver;
	}

	bool operator<(const ReadElement& right) const {
		return this->key < right.key;
	}
};

class WriteElement {
public:
	unsigned int key;
	Version *sourceObject, *newObject;

	WriteElement(unsigned int key, Version *source, Version *newOb) {
		this->key = key;
		this->sourceObject = source;
		this->newObject = newOb;
	}

	bool operator<(const WriteElement& right) const {
		return this->key < right.key;
	}
};

class GCElement {
public:
	unsigned int key;
	Version *ver;
	uint64_t wts;

	GCElement(unsigned int key, Version *ver, uint64_t wts) {
		this->key = key;
		this->ver = ver;
		this->wts = wts;
	}
};
