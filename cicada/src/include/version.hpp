#ifndef VERSION_HPP
#define VERSION_HPP

#include <atomic>
#include <cstdint>
#include <sys/time.h>
#include "timeStamp.hpp"

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
	std::atomic<uint64_t> rts;
	std::atomic<uint64_t> wts;
	
	unsigned int key;	//log what is this key.
	unsigned int val;			
	std::atomic<Version *> next;

	std::atomic<VersionStatus> status;	//commit record
	int8_t padding2[24];

	Version() {
		status.store(VersionStatus::pending, std::memory_order_release);
		next.store(nullptr, std::memory_order_release);
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
#endif	//	VERSION_HPP
