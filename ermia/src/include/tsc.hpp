#pragma once

#include <stdint.h>

uint64_t
rdtsc()
{
	uint64_t rax;
	uint64_t rdx;
	asm volatile ("rdtsc" : "=a"(rax), "=d"(rdx));
	return (rdx << 32) | rax;
}
