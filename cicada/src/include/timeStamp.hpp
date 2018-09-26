#pragma once
#include "tsc.hpp"
#include "barrier.h"

class TimeStamp {
public:
	uint64_t ts = 0;
	uint64_t localClock = 0;
	uint64_t clockBoost = 0;

	int8_t padding[40];

	TimeStamp() {}
	inline uint64_t get_ts() {
		return ts;
	}
	inline void set_ts(uint64_t &ts) {
		this->ts = ts;
	}
	
	inline void set_clockBoost(unsigned int CLOCK_PER_US) {
		//0を入れるか
		//1マイクロ秒相当のクロック数
		clockBoost = CLOCK_PER_US;
	}

	inline void generateTimeStampFirst(unsigned char tid) {
		localClock = rdtsc();
		ts = (localClock << 8) | tid;
	}
	
	inline void generateTimeStamp(unsigned char tid) {
		uint64_t tmp = rdtsc();
		memory_barrier();
		uint64_t elapsedTime = tmp - localClock;
		memory_barrier();

		//uint64_tをelapsedTimeの型に利用したい。
		//しかし、アンダーフローに対応できないと思う。
		//苦肉の策で0を入れておく。(後で改善したい)
		if (tmp < localClock) elapsedTime = 0;

		memory_barrier();
		
		//current local clock + 経過時間 + clockBoost
		localClock += elapsedTime;
		localClock += clockBoost;
		
		memory_barrier();
		ts = (localClock << 8) | tid;
	}
};
