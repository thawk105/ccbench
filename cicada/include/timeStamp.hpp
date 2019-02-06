#pragma once

#include "../../include/tsc.hpp"

class TimeStamp {
public:
	uint64_t ts = 0;
	uint64_t localClock = 0;
	uint64_t clockBoost = 0;
	uint8_t thid;

	int8_t padding[39];

	TimeStamp() {}
	inline uint64_t get_ts() {
		return ts;
	}
	inline void set_ts(uint64_t &ts) {
		this->ts = ts;
	}
	
	inline void set_clockBoost(unsigned int CLOCK_PER_US) {
		// set 0 or some value equivalent to 1 us.
		clockBoost = CLOCK_PER_US;
	}

	inline void generateTimeStampFirst(unsigned char tid) {
		localClock = rdtsc();
		ts = (localClock << 8) | tid;
		thid = tid;
	}
	
	inline void generateTimeStamp(unsigned char tid) {
		uint64_t tmp = rdtsc();
		uint64_t elapsedTime = tmp - localClock;
		if (tmp < localClock) elapsedTime = 0;
		
		//current local clock + elapsed time + clockBoost
		localClock += elapsedTime;
		localClock += clockBoost;
		
		ts = (localClock << 8) | tid;
	}
};
