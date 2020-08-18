/**
 * @file compiler.h
 */

#pragma once

namespace ccbench {

#define likely(x) __builtin_expect(!!(x), 1)    // NOLINT
#define unlikely(x) __builtin_expect(!!(x), 0)  // NOLINT

#define STRING(macro) #macro          // NOLINT
#define MAC2STR(macro) STRING(macro)  // NOLINT

} // namespace ccbench
