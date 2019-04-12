#pragma once

template <typename T>
T load(T& ptr) {
  return __atomic_load_n(&ptr, __ATOMIC_RELAXED);
}

template <typename T>
T loadAcquire(T& ptr) {
  return __atomic_load_n(&ptr, __ATOMIC_ACQUIRE);
}

template <typename T, typename T2>
void store(T& ptr, T2 val) {
  __atomic_store_n(&ptr, (T)val, __ATOMIC_RELAXED);
}

template <typename T, typename T2>
void storeRelease(T& ptr, T2 val) {
  __atomic_store_n(&ptr, (T)val, __ATOMIC_RELEASE);
}

