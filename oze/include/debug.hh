#pragma once
#include <execinfo.h>
#include <cassert>
#include <iostream>
#include <vector>

#define DEBUG_OUT_DIR "./out/"
#define DOT_COMMAND "/usr/bin/dot"

#ifdef MY_DEBUG
  #define ASSERT(expr) my_assert(expr)
#else
  #define ASSERT(ignore) ((void)0)
#endif

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

extern std::vector<std::string> my_backtrace();
extern void my_assert(bool expr);
