#pragma once

#include "inline.hh"

INLINE void compilerFence() { asm volatile("":: : "memory"); }
