#pragma once

#include "common.hpp"

#include "../../include/inline.hpp"

INLINE uint64_t atomicLoadGE();

INLINE void
atomicAddGE()
{
  uint64_t expected, desired;

  expected = atomicLoadGE();
  for (;;) {
    desired = expected + 1;
    if (__atomic_compare_exchange_n(&(GlobalEpoch.obj), &expected, desired, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) break;
  }
}

INLINE uint64_t
atomicLoadGE()
{
  uint64_t_64byte result = __atomic_load_n(&(GlobalEpoch.obj), __ATOMIC_ACQUIRE);
  return result.obj;
}

INLINE void
atomicStoreThLocalEpoch(unsigned int thid, uint64_t newval)
{
  __atomic_store_n(&(ThLocalEpoch[thid].obj), newval, __ATOMIC_RELEASE);
}
