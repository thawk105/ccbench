#pragma once

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <cstdarg>
#include <cstring>
#include <cstdint>
#include <cinttypes>
#include <cerrno>
#include <cassert>
#include <cstdio>
#include <cctype>
#include <stdexcept>
#include <string>
#include <sstream>
#include <vector>

inline void compiler_fence() { asm volatile("" ::: "memory");}

[[maybe_unused]]
inline
static
bool
chkClkSpan(const uint64_t start, const uint64_t stop, const uint64_t threshold)
{
  uint64_t diff = 0;
  diff = stop - start;
  if (diff > threshold) return true;
  else return false;
}

[[maybe_unused]]
inline
static
bool
chkClkSpanSec(const uint64_t start, const uint64_t stop, const unsigned int clocks_per_us, const uint64_t sec)
{
  uint64_t diff = 0;
  diff = stop - start;
  diff = diff / clocks_per_us / 1000 / 1000;
  if (diff > sec) return true;
  else return false;
}

class LibcError : public std::exception
{
private:
  std::string str_;
  static std::string generateMessage(int errnum, const std::string &msg) {
    std::string s(msg);
    const size_t BUF_SIZE = 1024;
    char buf[BUF_SIZE];
    ::snprintf(buf, 1024, " %d ", errnum);
    s += buf;
#ifdef Linux
    if (::strerror_r(errnum, buf, BUF_SIZE) != nullptr)
#endif // Linux
#ifdef Darwin
    if (::strerror_r(errnum, buf, BUF_SIZE) != 0)
#endif // Darwin
      s += buf;
    return s;
  }

public:
  explicit LibcError(int errnum = errno, const std::string &msg = "libc_error:") : str_(generateMessage(errnum, msg)) {}
};

