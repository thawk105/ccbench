#pragma once
#include "common.hpp"
#include "../../../include/inline.hpp"

INLINE uint64_t_64byte
loadAcquireGE()
{
	return __atomic_load_n(&(GlobalEpoch.obj), __ATOMIC_ACQUIRE);
}

INLINE void
atomicAddGE()
{
	uint64_t_64byte expected, desired;

	expected = loadAcquireGE();
	for (;;) {
		desired.obj = expected.obj + 1;
		if (__atomic_compare_exchange_n(&(GlobalEpoch.obj), &(expected.obj), desired.obj, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) break;
	}
}

