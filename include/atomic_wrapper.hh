/**
 * This file was created with reference to the following URL.
 * Special thanks to Mr. Hoshino.
 * https://github.com/starpos/oltp-cc-bench/blob/master/include/atomic_wrapper.hpp
 */

#pragma once

template<typename T>
T load(T &ptr) {
  return __atomic_load_n(&ptr, __ATOMIC_RELAXED);
}

template<typename T>
T loadAcquire(T &ptr) {
  return __atomic_load_n(&ptr, __ATOMIC_ACQUIRE);
}

template<typename T, typename T2>
void store(T &ptr, T2 val) {
  __atomic_store_n(&ptr, (T) val, __ATOMIC_RELAXED);
}

template<typename T, typename T2>
void storeRelease(T &ptr, T2 val) {
  __atomic_store_n(&ptr, (T) val, __ATOMIC_RELEASE);
}

template<typename T, typename T2>
bool compareExchange(T &m, T &before, T2 after) {
  return __atomic_compare_exchange_n(&m, &before, (T) after, false,
                                     __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
}
