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
	std::atomic<uint64_t> wts;				// Version creation stamp, c(V)
	std::atomic<uint64_t> rts;	
	Version *prev;	// Pointer to overwritten version

	std::atomic<uint64_t> val;
	std::atomic<VersionStatus> status;
	int8_t padding[23];

	Version() {
		rts.store(0, std::memory_order_release);
		wts.store(0, std::memory_order_release);
		prev = nullptr;
		status.store(VersionStatus::inFlight, std::memory_order_release);
	}
};

#endif	//	VERSION_HPP
