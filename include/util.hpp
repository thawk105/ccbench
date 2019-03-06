#pragma once

#include <stdio.h>
#include <sys/time.h>

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

static
void
writeValGenerator(char *writeVal, int val_size, int thid)
{
  // generate write value for this thread.
  int num(thid), digit(1);
  while (num != 0) {
    num /= 10;
    if (num != 0) ++digit;
  }
  char thidString[digit];
  sprintf(thidString, "%d", thid); 
  for (int i = 0; i < val_size;) {
    for (int j = 0; j < digit; ++j) {
      writeVal[i] = thidString[j];
      ++i;
      if (i == val_size - 2) {
        break;
      }
    }
  }
  writeVal[val_size - 1] = '\0';
  // -----
}
