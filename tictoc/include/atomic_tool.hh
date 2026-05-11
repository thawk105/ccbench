#pragma once

#include "common.hh"

#include "../../include/inline.hh"

INLINE uint64_t atomicLoadGE();

INLINE void atomicAddGE() {
  uint64_t expected, desired;

  expected = atomicLoadGE();
  for (;;) {
    desired = expected + 1;
    if (__atomic_compare_exchange_n(&(GlobalEpoch.obj_), &expected, desired,
                                    false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
      break;
  }
}

INLINE uint64_t atomicLoadGE() {
  uint64_t_64byte result =
          __atomic_load_n(&(GlobalEpoch.obj_), __ATOMIC_ACQUIRE);
  return result.obj_;
}

INLINE void atomicStoreThLocalEpoch(unsigned int thid, uint64_t newval) {
  __atomic_store_n(&(ThLocalEpoch[thid].obj_), newval, __ATOMIC_RELEASE);
}
