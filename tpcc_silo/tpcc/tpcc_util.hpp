#pragma once

#include <string_view>
#include <cstring>
#include <cinttypes>
#include <sstream>
#include <cstdio>
#include <cassert>
#include <vector>
#include "../include/random.hh"


template <typename Int>
Int byteswap(Int in)
{
  switch (sizeof(Int)) {
    case 1:
      return in;
    case 2:
      return __builtin_bswap16(in);
    case 4:
      return __builtin_bswap32(in);
    case 8:
      return __builtin_bswap64(in);
    default:
      assert(false);
  };
}

template<typename Int>
void assign_as_bigendian(Int value, char *out)
{
  Int tmp = byteswap(value);
  ::memcpy(out, &tmp, sizeof(tmp));
}

template <typename Int>
void parse_bigendian(const char* in, Int& out)
{
  Int tmp;
  ::memcpy(&tmp, in, sizeof(tmp));
  out = byteswap(tmp);
}


template<typename T>
std::string_view struct_str_view(const T &t) {
  return std::string_view(reinterpret_cast<const char *>(&t), sizeof(t));
}


/**
 * Better strncpy().
 * out buffer will be null-terminated.
 * returned value is written size excluding the last null character.
 */
inline size_t copy_cstr(char* out, const char* in, size_t out_buf_size)
{
  if (out_buf_size == 0) return 0;
  size_t i = 0;
  while (i < out_buf_size - 1) {
    if (in[i] == '\0') break;
    out[i] = in[i];
    i++;
  }
  out[i] = '\0';
  return i;
}


/**
 * for debug.
 */
inline std::string str_view_hex(std::string_view sv) {
  std::stringstream ss;
  char buf[3];
  for (auto &&i : sv) {
    ::snprintf(buf, sizeof(buf), "%02x", i);
    ss << buf;
  }
  return ss.str();
}


template<typename T>
inline std::string_view str_view(const T &t) {
  return std::string_view(reinterpret_cast<const char *>(&t), sizeof(t));
}


namespace TPCC {


struct Xoroshiro128PlusWrapper : Xoroshiro128Plus
{
  Xoroshiro128PlusWrapper() { init(); }
};


/**
 * All thread can use 64bit random number generator.
 */
inline thread_local Xoroshiro128PlusWrapper rand_;


/**
 * returned value is in [min, max]. (both-side inclusive)
 */
inline uint64_t random_number(uint64_t min, uint64_t max)
{
  assert(min <= max);
  assert(max < UINT64_MAX);

  return rand_() % (max - min + 1) + min;
}


constexpr uint64_t get_constant_for_non_uniform_random(uint64_t A, bool is_load)
{
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
  constexpr uint64_t C_FOR_C_LAST_IN_LOAD = 250;
  constexpr uint64_t C_FOR_C_LAST_IN_RUN = 150;
  constexpr uint64_t C_FOR_C_ID = 987;
  constexpr uint64_t C_FOR_OL_I_ID = 5987;

  static_assert(C_FOR_C_LAST_IN_LOAD <= 255);
  static_assert(C_FOR_C_LAST_IN_RUN <= 255);
  constexpr uint64_t delta = C_FOR_C_LAST_IN_LOAD - C_FOR_C_LAST_IN_RUN;
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


template <uint64_t A, bool IS_LOAD = false>
uint64_t non_uniform_random(uint64_t x, uint64_t y)
{
  constexpr uint64_t C = get_constant_for_non_uniform_random(A, IS_LOAD);
  if (C == UINT64_MAX) {
    throw std::runtime_error("non_uniform_random() bug");
  }
  return (((random_number(0, A) | random_number(x, y)) + C) % (y - x + 1)) + x;
}


/**
 * out buffer size must be maxlen + 1 or more.
 * out buffer will be null-terminated.
 * returned value is length of the string excluding the last null-value.
 */
template <bool is_number_only>
size_t random_string_detail(size_t min_len, size_t max_len, char* out)
{
  const char c[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  const size_t max_idx = is_number_only ? 9 : (sizeof(c) - 1);

  size_t len = random_number(min_len, max_len);
  for (size_t i = 0; i < len; i++) {
    out[i] = c[random_number(0, max_idx)];
  }
  out[len] = '\0';
  return len;
}

inline size_t random_alpha_string(size_t min_len, size_t max_len, char* out)
{
  return random_string_detail<false>(min_len, max_len, out);
}

inline size_t random_number_string(size_t min_len, size_t max_len, char* out)
{
  return random_string_detail<true>(min_len, max_len, out);
}

/**
 * out buffer size must be 9 + 1.
 * out buffer will be null-terminated.
 *
 * See section 4.3.2.7 of TPC-C specification v5.11.0.
 */
inline void random_zip_code(char* out)
{
  random_number_string(4, 4, &out[0]);
  out[4] = '1';
  out[5] = '1';
  out[6] = '1';
  out[7] = '1';
  out[8] = '1';
  out[9] = '\0';
}


inline void make_address(char* str1, char* str2, char* city, char* state, char* zip)
{
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
inline size_t make_c_last(size_t num, char* out)
{
  const char* chunk[] = {"BAR", "OUGHT", "ABLE", "PRI", "PRES", "ESE", "ANTI", "CALLY", "ATION", "EING"};
  assert(num < 1000);

  constexpr size_t buf_size = 16;
  size_t len = 0;
  for (size_t i : {num / 100, (num / 10) % 10, num % 10}) {
    len += copy_cstr(&out[len], chunk[i], buf_size - len);
  }
  assert(len <= 15);
  out[len] = '\0';
  return len;
}


/**
 * This is 0-origin.
 */
struct IsOriginal
{
private:
  std::vector<bool> bitvec_;
  size_t nr_total_;

public:
  IsOriginal(size_t nr_total, size_t nr_original)
    : bitvec_(nr_total), nr_total_(nr_total) {
    assert(nr_total > nr_original);
    // CAUSION: nr_original is too large. the following code will be very very slow.
    for (size_t i = 0; i < nr_original; i++) {
      size_t id;
      do {
        id = random_number(0, nr_total - 1);
      } while (bitvec_[id]);
      bitvec_[id] = true;
    }
  }

  bool operator[](size_t id) const {
    assert(id < nr_total_);
    return bitvec_[id];
  }
};


inline void make_original(char* target, size_t len)
{
  assert(len >= 8);
  const char orig[] = "ORIGINAL";
  size_t pos = random_number(0, len - 8);
  for (size_t i = 0; i < 8; i++) {
    target[pos + i] = orig[i];
  }
}


struct Permutation
{
  std::vector<size_t> perm_;

  /**
   * [min, max].
   */
  Permutation(size_t min, size_t max) : perm_(max - min + 1) {
    assert(min < max);
    size_t i = min;
    for (size_t& val : perm_) {
      val = i;
      i++;
    }
    assert(i - 1 == max);

    const size_t s = perm_.size();
    for (size_t i = 0; i < s - 1; i++) {
      size_t j = random_number(0, s - i - 1);
      assert(i + j < s);
      if (j != 0) std::swap(perm_[i], perm_[i + j]);
    }
  }

  size_t size() const { return perm_.size(); }

  // CAUSION: i is 0-origin.
  size_t operator[](size_t i) const {
    assert(i < perm_.size());
    return perm_[i];
  }
};


} // namespace
