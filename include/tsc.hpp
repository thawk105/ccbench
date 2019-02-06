#pragma once

#include <stdint.h>

static uint64_t rdtsc() {
	uint64_t rax;
	uint64_t rdx;

	asm volatile("" ::: "memory");
	asm volatile("rdtsc" : "=a"(rax), "=d"(rdx));
	asm volatile("" ::: "memory");

	return (rdx << 32) | rax;
}
