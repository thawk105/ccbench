#pragma once

#include "../../include/inline.hh"

#include "common.hh"

INLINE uint64_t_64byte loadAcquireGE() {
  return __atomic_load_n(&(GlobalEpoch.obj_), __ATOMIC_ACQUIRE);
}

INLINE void atomicAddGE() {
  uint64_t_64byte expected, desired;

  expected = loadAcquireGE();
  for (;;) {
    desired.obj_ = expected.obj_ + 1;
    if (__atomic_compare_exchange_n(&(GlobalEpoch.obj_), &(expected.obj_),
                                    desired.obj_, false, __ATOMIC_ACQ_REL,
                                    __ATOMIC_ACQUIRE))
      break;
  }
}
