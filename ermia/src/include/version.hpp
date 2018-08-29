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
	std::atomic<uint64_t> cstamp;				// Version creation stamp, c(V)
	std::atomic<uint64_t> pstamp;				// Version access stamp, η(V)
	std::atomic<uint64_t> sstamp;				// Version successor stamp, pi(V)
	Version *prev;	// Pointer to overwritten version
	Version *committed_prev;	// Pointer to the next committed version

	std::atomic<uint64_t> val;
	std::atomic<uint64_t> readers;	// summarize all of V's readers.
	std::atomic<VersionStatus> status;
	int8_t padding[71];

	Version() {
		pstamp.store(0, std::memory_order_release);
		sstamp.store(UINT64_MAX, std::memory_order_release);
		status.store(VersionStatus::inFlight, std::memory_order_release);
		readers.store(0, std::memory_order_release);
	}
};

#endif	//	VERSION_HPP
