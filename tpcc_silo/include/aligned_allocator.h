/**
 * @file aligned_allocator.h
 * @brief atomic wrapper of builtin function.
 */

#pragma once

namespace ccbench {

#include <stdlib.h>
#include <cstdlib>
#include <iostream>
#include <type_traits>
#include <vector>

/**
 * @return pointer. (success) allocated pointer. (fail) nullptr.
 */
template<typename T = void>
static inline T *
aligned_malloc(std::size_t n_bytes, std::size_t alignment = alignof(T)) noexcept {
  void *p;
  return reinterpret_cast<T *>(posix_memalign(&p, alignment, n_bytes) == 0 ? p : nullptr);
}

template<typename T>
static inline T *
aligned_alloc_array(std::size_t size, std::size_t alignment = alignof(T)) noexcept {
  return aligned_malloc<T>(size * sizeof(T), alignment);
}


static inline void
aligned_free(void *ptr) noexcept {
  std::free(ptr);
}

template<typename T, std::size_t k_alignment = alignof(T)>
class aligned_allocator {
public:
  using value_type = T;
  using size_type = std::size_t;
  using pointer = typename std::add_pointer<value_type>::type;
  using const_pointer = typename std::add_pointer<const value_type>::type;

  template<class U>
  struct rebind {
    using other = aligned_allocator<U, k_alignment>;
  };

  aligned_allocator() noexcept {}

  template<typename U>
  aligned_allocator(const aligned_allocator<U, k_alignment> &) noexcept {}

  pointer allocate(size_type n, const_pointer = nullptr) const {
    auto p = aligned_alloc_array<value_type>(n, k_alignment);
    if (p == nullptr) {
      throw std::bad_alloc{};
    }
    return p;
  }

  void deallocate(pointer p, size_type) const noexcept {
    aligned_free(p);
  }
};  // class aligned_allocator


template<typename T, std::size_t k_alignment1, typename U, std::size_t k_alignment2>
static inline bool
operator==(const aligned_allocator<T, k_alignment1> &, const aligned_allocator<U, k_alignment2> &) noexcept {
  return k_alignment1 == k_alignment2;
}


template<typename T, std::size_t k_alignment1, typename U, std::size_t k_alignment2>
static inline bool
operator!=(const aligned_allocator<T, k_alignment1> &lhs, const aligned_allocator<U, k_alignment2> &rhs) noexcept {
  return !(lhs == rhs);
}

} // namespace ccbench
