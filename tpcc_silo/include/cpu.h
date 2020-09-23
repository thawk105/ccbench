/**
 * @file cpu.h
 */

#pragma once

#include <cpuid.h>
#include <sched.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include <atomic>
#include <cstdint>
#include <iostream>

#include "../../include/cache_line_size.hh"

namespace ccbench {

#ifdef CCBENCH_LINUX

[[maybe_unused]] static void setThreadAffinity(const int my_id) {
  using namespace std;
  static std::atomic<int> n_processors(-1);
  int local_n_processors = n_processors.load(memory_order_acquire);
  for (;;) {
    if (local_n_processors != -1) {
      break;
    }
    int desired = sysconf(_SC_NPROCESSORS_CONF);  // NOLINT
    if (n_processors.compare_exchange_strong(local_n_processors, desired,
                                             memory_order_acq_rel,
                                             memory_order_acquire)) {
      break;
    }
  }

  pid_t pid = syscall(SYS_gettid);  // NOLINT
  cpu_set_t cpu_set;

  CPU_ZERO(&cpu_set);
  CPU_SET(my_id % local_n_processors, &cpu_set);  // NOLINT

  if (sched_setaffinity(pid, sizeof(cpu_set_t), &cpu_set) != 0) {
    std::cout << __FILE__ << " : " << __LINE__ << " : fatal error."
              << std::endl;
    std::abort();
  }
}

[[maybe_unused]] static void setThreadAffinity(const cpu_set_t id) {
  pid_t pid = syscall(SYS_gettid);  // NOLINT

  if (sched_setaffinity(pid, sizeof(cpu_set_t), &id) != 0) {
    std::cout << __FILE__ << " : " << __LINE__ << " : fatal error."
              << std::endl;
    std::abort();
  }
}

[[maybe_unused]] static cpu_set_t getThreadAffinity() {  // NOLINT
  pid_t pid = syscall(SYS_gettid);                       // NOLINT
  cpu_set_t result;

  if (sched_getaffinity(pid, sizeof(cpu_set_t), &result) != 0) {
    std::cout << __FILE__ << " : " << __LINE__ << " : fatal error."
              << std::endl;
    std::abort();
  }

  return result;
}


inline int cached_sched_getcpu()
{
    thread_local int value = ::sched_getcpu();
    return value;
}


#endif  // CCBENCH_LINUX

}  // namespace ccbench
