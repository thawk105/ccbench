/**
 * @file fence.h
 */

#pragma once

#include "inline.h"

namespace ccbench {

INLINE void compilerFence() { asm volatile("":: : "memory"); }

} // namespace ccbench

