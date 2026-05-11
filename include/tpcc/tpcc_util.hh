#pragma once

#include <string_view>
#include <cstring>
#include <cinttypes>
#include <sstream>
#include <cstdio>
#include <cassert>
#include <vector>

#include "../random.hh"

struct Xoroshiro128PlusWrapper : Xoroshiro128Plus {
  Xoroshiro128PlusWrapper() { init(); }
};


/**
 * All thread can use 64bit random number generator.
 */
inline std::uint64_t random_64bits()
{
  thread_local Xoroshiro128PlusWrapper rand;
  return rand();
}


/**
 * returned value is in [min, max]. (both-side inclusive)
 */
inline std::uint64_t random_int(std::uint64_t min, std::uint64_t max)
{
  assert(min <= max);
  assert(max < UINT64_MAX);

  return random_64bits() % (max - min + 1) + min;
}


inline double random_double(std::uint64_t min, std::uint64_t max, std::size_t divider)
{
  return random_int(min, max) / (double)divider;
}


inline void fill_random(void* out, size_t size)
{
  char* p = reinterpret_cast<char*>(out);

  uint64_t buf;
  while (size >= sizeof(uint64_t)) {
    buf = random_64bits();
    ::memcpy(p, &buf, sizeof(uint64_t));
    p += sizeof(uint64_t);
    size -= sizeof(uint64_t);
  }
  if (size > 0) {
    buf = random_64bits();
    ::memcpy(p, &buf, size);
  }
}


constexpr std::uint64_t get_constant_for_non_uniform_random(std::uint64_t A, bool is_load) {
  /*
   * From section 2.1.6 of TPC-C specifiation v5.11.0:
   *
   * A is a constant chosen according to the size of the range [x .. y]
   * for C_LAST, the range is [0 .. 999] and A = 255
   * for C_ID, the range is [1 .. 3000] and A = 1023
   * for OL_I_ID, the range is [1 .. 100000] and A = 8191
   *
   * C is a run-time constant randomly chosen within [0 .. A] that can be varied without altering performance.
   * The same C value, per field (C_LAST, C_ID, and OL_I_ID), must be used by all emulated terminals.
   *
   * In order that the value of C used for C_LAST does not alter performance the following must be true:
   * Let C-Load be the value of C used to generate C_LAST when populating the database. C-Load is a value
   * in the range of [0..255] including 0 and 255.
   *
   * Let C-Run be the value of C used to generate C_LAST for the measurement run.
   * Let C-Delta be the absolute value of the difference between C-Load and C-Run. C-Delta must be a value in
   * the range of [65..119] including the values of 65 and 119 and excluding the value of 96 and 112.
   */
  constexpr std::uint64_t C_FOR_C_LAST_IN_LOAD = 250;
  constexpr std::uint64_t C_FOR_C_LAST_IN_RUN = 150;
  constexpr std::uint64_t C_FOR_C_ID = 987;
  constexpr std::uint64_t C_FOR_OL_I_ID = 5987;

  static_assert(C_FOR_C_LAST_IN_LOAD <= 255);
  static_assert(C_FOR_C_LAST_IN_RUN <= 255);
  constexpr std::uint64_t delta = C_FOR_C_LAST_IN_LOAD - C_FOR_C_LAST_IN_RUN;
  static_assert(65 <= delta && delta <= 119 && delta != 96 && delta != 112);
  static_assert(C_FOR_C_ID <= 1023);
  static_assert(C_FOR_OL_I_ID <= 8191);

  switch (A) {
    case 255:
      return is_load ? C_FOR_C_LAST_IN_LOAD : C_FOR_C_LAST_IN_RUN;
    case 1023:
      return C_FOR_C_ID;
    case 8191:
      return C_FOR_OL_I_ID;
    default:
      return UINT64_MAX; // BUG
  }
}


template<std::uint64_t A, bool IS_LOAD = false>
std::uint64_t non_uniform_random(std::uint64_t x, std::uint64_t y) {
  constexpr std::uint64_t C = get_constant_for_non_uniform_random(A, IS_LOAD);
  if (C == UINT64_MAX) {
    throw std::runtime_error("non_uniform_random() bug");
  }
  return (((random_int(0, A) | random_int(x, y)) + C) % (y - x + 1)) + x;
}


/**
 * out buffer size must be maxlen + 1 or more.
 * out buffer will be null-terminated.
 * returned value is length of the string excluding the last null-value.
 */
template<bool is_number_only>
std::size_t random_string_detail(std::size_t min_len, std::size_t max_len, char *out) {
  const char c[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  const std::size_t max_idx = is_number_only ? 9 : (sizeof(c) - 1);

  std::size_t len = random_int(min_len, max_len);
#if 0
  for (std::size_t i = 0; i < len; i++) {
    out[i] = c[random_int(0, max_idx)];
  }
#else
  fill_random(out, len);
  for (size_t i = 0; i < len; i++) {
    out[i] = c[out[i] % max_idx];
  }
#endif
  out[len] = '\0';
  return len;
}

inline std::size_t random_alpha_string(std::size_t min_len, std::size_t max_len, char *out) {
  return random_string_detail<false>(min_len, max_len, out);
}

inline std::size_t random_number_string(std::size_t min_len, std::size_t max_len, char *out) {
  return random_string_detail<true>(min_len, max_len, out);
}

/**
 * out buffer size must be 9 + 1.
 * out buffer will be null-terminated.
 *
 * See section 4.3.2.7 of TPC-C specification v5.11.0.
 */
inline void random_zip_code(char *out) {
  random_number_string(4, 4, &out[0]);
  out[4] = '1';
  out[5] = '1';
  out[6] = '1';
  out[7] = '1';
  out[8] = '1';
  out[9] = '\0';
}


inline void make_address(char *str1, char *str2, char *city, char *state, char *zip) {
  random_alpha_string(10, 20, str1); // street 1.
  random_alpha_string(10, 20, str2); // street 2.
  random_alpha_string(10, 20, city);
  random_alpha_string(2, 2, state);
  random_zip_code(zip);
}


/**
 * num must be in [0, 999].
 * out buffer size must 15 + 1 or more.
 * returned value is the length of the c_last name excluding the last null character.
 */
inline std::size_t make_c_last(std::size_t num, char *out) {
  const char *chunk[] = {"BAR", "OUGHT", "ABLE", "PRI", "PRES", "ESE", "ANTI", "CALLY", "ATION", "EING"};
  assert(num < 1000);

  constexpr std::size_t buf_size = 16;
  std::size_t len = 0;
  for (std::size_t i : {num / 100, (num / 10) % 10, num % 10}) {
    len += copy_cstr(&out[len], chunk[i], buf_size - len);
  }
  assert(len <= 15);
  out[len] = '\0';
  return len;
}


/**
 * This is 0-origin.
 */
struct IsOriginal {
private:
  std::vector<bool> bitvec_;
  std::size_t nr_total_;

public:
  IsOriginal(std::size_t nr_total, std::size_t nr_original)
          : bitvec_(nr_total), nr_total_(nr_total) {
    assert(nr_total > nr_original);
    // CAUSION: nr_original is too large. the following code will be very very slow.
    for (std::size_t i = 0; i < nr_original; i++) {
      std::size_t id;
      do {
        id = random_int(0, nr_total - 1);
      } while (bitvec_[id]);
      bitvec_[id] = true;
    }
  }

  bool operator[](std::size_t id) const {
    assert(id < nr_total_);
    return bitvec_[id];
  }
};


inline void make_original(char *target, std::size_t len) {
  assert(len >= 8);
  const char orig[] = "ORIGINAL";
  std::size_t pos = random_int(0, len - 8);
  for (std::size_t i = 0; i < 8; i++) {
    target[pos + i] = orig[i];
  }
}


struct Permutation {
  std::vector<std::size_t> perm_;

  /**
   * [min, max].
   */
  Permutation(std::size_t min, std::size_t max) : perm_(max - min + 1) {
    assert(min < max);
    {
      std::size_t i = min;
      for (std::size_t &val : perm_) {
        val = i;
        i++;
      }
      assert(i - 1 == max);
    }

    const std::size_t s = perm_.size();
    for (std::size_t i = 0; i < s - 1; i++) {
      std::size_t j = random_int(0, s - i - 1);
      assert(i + j < s);
      if (j != 0) std::swap(perm_[i], perm_[i + j]);
    }
  }

  [[nodiscard]] std::size_t size() const { return perm_.size(); }

  // CAUSION: i is 0-origin.
  std::size_t operator[](std::size_t i) const {
    assert(i < perm_.size());
    return perm_[i];
  }
};


alignas(CACHE_LINE_SIZE) inline time_t TPCCTimestamp;
[[maybe_unused]] inline time_t get_lightweight_timestamp()
{
    return loadAcquire(TPCCTimestamp);
}
