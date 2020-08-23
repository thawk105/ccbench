/**
 * @file memory.h
 */

#pragma once

#include <sys/resource.h>

#include <cstdio>
#include <ctime>
#include <iostream>

namespace ccbench {

static void displayRusageRUMaxrss() {  // NOLINT
  struct rusage r{};
  if (getrusage(RUSAGE_SELF, &r) != 0) {
    std::cout << __FILE__ << " : " << __LINE__ << " : fatal error."
              << std::endl;
    std::abort();
  }
  printf("maxrss:\t%ld kB\n", r.ru_maxrss);  // NOLINT
}

}
