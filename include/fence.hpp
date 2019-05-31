#pragma once

#include "inline.hpp"

INLINE void compiler_fence() { asm volatile("" ::: "memory"); }
