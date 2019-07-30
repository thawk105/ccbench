#pragma once

#include <cpuid.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include "debug.hh"

#define CPUID(INFO, LEAF, SUBLEAF) \
  __cpuid_count(LEAF, SUBLEAF, INFO[0], INFO[1], INFO[2], INFO[3])

#define GETCPU(CPU)                                                 \
  {                                                                 \
    uint32_t CPUInfo[4];                                            \
    CPUID(CPUInfo, 1, 0);                                           \
    /* CPUInfo[1] is EBX, bits 24-31 are APIC ID */                 \
    if ((CPUInfo[3] & (1 << 9)) == 0) {                             \
      CPU = -1; /* no APIC on chip */                               \
    } else {                                                        \
      CPU = (unsigned)CPUInfo[1] >> 24;                             \
      /*unsigned int cores = ((unsigned)CPUInfo[1] >> 16) & 0xff;*/ \
      /*if ((CPUInfo[3] & (1 << 28)) == 1) printf("HTT\n");*/       \
      /*printf("total core number : %d\n", cores);*/                \
    }                                                               \
    if (CPU < 0) CPU = 0;                                           \
  }

#ifdef Linux
static void setThreadAffinity(const int myid) {
  pid_t pid = syscall(SYS_gettid);
  cpu_set_t cpu_set;

  CPU_ZERO(&cpu_set);
  CPU_SET(myid % sysconf(_SC_NPROCESSORS_CONF), &cpu_set);

  if (sched_setaffinity(pid, sizeof(cpu_set_t), &cpu_set) != 0) ERR;

  // printf("thread affinity (id==%d) [ok]\n", myid);
  return;
}
#endif  // Linux
