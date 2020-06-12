#pragma once

#include <stdint.h>

[[maybe_unused]] static uint64_t
rdtsc() {
  uint64_t rax;
  uint64_t rdx;

  asm volatile("rdtsc" : "=a"(rax), "=d"(rdx));
  // store higher bits to rdx, lower bits to rax.
  // シリアライズ命令でないため，全ての先行命令を待機せずに
  // カウンタ値を読み込む．同様に後続命令が読み取り命令を追い越すことを許容する．

  return (rdx << 32) | rax;
}

[[maybe_unused]] static uint64_t
rdtsc_serial() {
  uint64_t rax;
  uint64_t rdx;

  asm volatile("cpuid":: : "rax", "rbx", "rcx", "rdx");
  asm volatile("rdtsc" : "=a"(rax), "=d"(rdx));

  return (rdx << 32) | rax;
}

[[maybe_unused]] static uint64_t
rdtscp() {
  uint64_t rax;
  uint64_t rdx;
  uint32_t aux;
  asm volatile("rdtscp" : "=a"(rax), "=d"(rdx), "=c"(aux)::);
  // store EDX:EAX.
  // 全ての先行命令を待機してからカウンタ値を読み取る．ただし，後続命令は
  // 同読み取り操作を追い越す可能性がある．

  return (rdx << 32) | rax;
}
