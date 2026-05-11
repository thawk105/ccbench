/**
 * This file was created with reference to the following URL.
 * Special thanks to Mr. Hoshino.
 * https://github.com/starpos/oltp-cc-bench/blob/master/include/atomic_wrapper.hpp
 */

#pragma once

/**
 * @brief atomic relaxed load.
 */
template<typename T>
[[maybe_unused]] static T load(T &ptr) {
  return __atomic_load_n(&ptr, __ATOMIC_RELAXED);
}

template<typename T>
[[maybe_unused]] static T load(T *ptr) {
  return __atomic_load_n(ptr, __ATOMIC_RELAXED);
}

template<typename T>
[[maybe_unused]] static T loadRelaxed(T &ptr) {
  return __atomic_load_n(&ptr, __ATOMIC_RELAXED);
}

template<typename T>
[[maybe_unused]] static T loadRelaxed(T *ptr) {
  return __atomic_load_n(ptr, __ATOMIC_RELAXED);
}

template<typename T>
static T loadAcquire(T &ptr) {
  return __atomic_load_n(&ptr, __ATOMIC_ACQUIRE);
}

/**
 * @brief atomic relaxed store.
 */
template<typename T, typename T2>
[[maybe_unused]] static void store(T &ptr, T2 val) {
  __atomic_store_n(&ptr, (T) val, __ATOMIC_RELAXED);
}

template<typename T, typename T2>
[[maybe_unused]] static void storeRelaxed(T &ptr, T2 val) {
  __atomic_store_n(&ptr, (T) val, __ATOMIC_RELAXED);  // NOLINT
}

template<typename T, typename T2>
[[maybe_unused]] static void storeRelaxed(T *ptr, T2 val) {
  __atomic_store_n(ptr, (T) val, __ATOMIC_RELAXED);  // NOLINT
}

/**
 * @brief atomic release store.
 */
template<typename T, typename T2>
static void storeRelease(T &ptr, T2 val) {
  __atomic_store_n(&ptr, (T) val, __ATOMIC_RELEASE);  // NOLINT
}

/**
 * @brief atomic acq-rel cas.
 */
template<typename T, typename T2>
static bool compareExchange(T &m, T &before, T2 after) {
  return __atomic_compare_exchange_n(&m, &before, (T) after, false,
                                     __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
}

template<typename Int1, typename Int2>
static Int1 fetchAdd(Int1 &m, Int2 v, int memorder = __ATOMIC_ACQ_REL) {
  return __atomic_fetch_add(&m, v, memorder);
}
