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
	
	int key;	//log what is this key.
	int val;			
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

#endif	//	VERSION_HPP
