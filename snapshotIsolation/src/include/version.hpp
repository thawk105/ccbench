#ifndef VERSION_HPP
#define VERSION_HPP

#include <atomic>
#include <cstdint>

enum class VersionStatus : uint8_t {
	inFlight,
	committed,
	aborted,
};

class Version {
public:
	uint64_t wts;				// Version creation stamp, c(V)
	std::atomic<uint64_t> rts;	
	Version *prev;	// Pointer to overwritten version
	Version *committed_prev;

	unsigned int val;
	std::atomic<VersionStatus> status;
	int8_t padding[23];

	Version() {
		rts.store(0, std::memory_order_release);
		wts.store(0, std::memory_order_release);
		prev = nullptr;
		status.store(VersionStatus::inFlight, std::memory_order_release);
	}
	Version(unsigned int val) {
		this->val = val;
		rts.store(0, std::memory_order_release);
		wts.store(0, std::memory_order_release);
		prev = nullptr;
		status.store(VersionStatus::inFlight, std::memory_order_release);
	}
};

class ReadElement {
public:
	unsigned int key;
	Version *ver;
	uint64_t rts;

	ReadElement(unsigned int key, Version *ver, uint64_t rts) {
		this->key = key;
		this->ver = ver;
		this->rts = rts;
	}
};

class WriteElement {
public:
	unsigned int key;
	Version *source, *newOb;

	WriteElement(unsigned int key, Version *source, Version *newOb) {
		this->key = key;
		this->source = source;
		this->newOb = newOb;
	}
};

#endif	//	VERSION_HPP
