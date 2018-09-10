#ifndef TSC_HPP
#define TSC_HPP

#include <stdint.h>

uint64_t
rdtsc()
{
	uint64_t rax;
	uint64_t rdx;
	asm volatile ("rdtsc" : "=a"(rax), "=d"(rdx));
	return (rdx << 32) | rax;
}

#endif	//	TSC_HPP
