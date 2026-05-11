#pragma once

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>
#include <xmmintrin.h>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cctype>
#include <cerrno>
#include <cinttypes>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "debug.hh"
#include "procedure.hh"
#include "random.hh"
#include "result.hh"
#include "tsc.hh"
#include "zipf.hh"

// class
class LibcError : public std::exception {
private:
  std::string str_;

  static std::string generateMessage(int errnum, const std::string &msg) {
    std::string s(msg);
    const size_t BUF_SIZE = 1024;
    char buf[BUF_SIZE];
    ::snprintf(buf, 1024, " %d ", errnum);
    s += buf;
#ifdef Linux
    if (::strerror_r(errnum, buf, BUF_SIZE) != nullptr)
#endif  // Linux
#ifdef Darwin
    if (::strerror_r(errnum, buf, BUF_SIZE) != 0)
#endif  // Darwin
    s += buf;
    return s;
  }

public:
  explicit LibcError(int errnum = errno, const std::string &msg = "libc_error:")
          : str_(generateMessage(errnum, msg)) {}
};

// function
[[maybe_unused]] extern bool chkSpan(struct timeval &start, struct timeval &stop, long threshold);

extern size_t decideParallelBuildNumber(size_t tuple_num);

extern void displayProcedureVector(std::vector <Procedure> &pro);

extern void displayRusageRUMaxrss();

extern bool isReady(const std::vector<char> &readys);

extern void readyAndWaitForReadyOfAllThread(std::atomic <size_t> &running, const size_t thnm);

extern void sleepMs(size_t ms);

extern void sleepMicroSec(size_t ms);

extern void waitForReady(const std::vector<char> &readys);

extern void waitForReadyOfAllThread(std::atomic <size_t> &running, const size_t thnm);

//----------
// After this line, intending to force inline function.

[[maybe_unused]] inline static bool chkClkSpan(const uint64_t start,
                                               const uint64_t stop,
                                               const uint64_t threshold) {
  uint64_t diff = 0;
  diff = stop - start;
  if (diff > threshold)
    return true;
  else
    return false;
}

[[maybe_unused]] inline static bool chkClkSpanSec(
        const uint64_t start, const uint64_t stop, const unsigned int clocks_per_us,
        const uint64_t sec) {
  uint64_t diff = 0;
  diff = stop - start;
  diff = diff / clocks_per_us / 1000 / 1000;
  if (diff > sec)
    return true;
  else
    return false;
}

// inline static void makeProcedure(std::vector <Procedure> &pro, Xoroshiro128Plus &rnd,
//                                  FastZipf &zipf, size_t tuple_num, size_t max_ope,
//                                  size_t thread_num, size_t rratio, bool rmw, bool ycsb,
//                                  bool partition, size_t thread_id, [[maybe_unused]]Result &res) {
// #if ADD_ANALYSIS
//   uint64_t start = rdtscp();
// #endif
//   pro.clear();
//   bool ronly_flag(true), wonly_flag(true);
//   for (size_t i = 0; i < max_ope; ++i) {
//     uint64_t tmpkey;
//     // decide access destination key.
//     if (ycsb) {
//       if (partition) {
//         size_t block_size = tuple_num / thread_num;
//         tmpkey = (block_size * thread_id) + (zipf() % block_size);
//       } else {
//         tmpkey = zipf() % tuple_num;
//       }
//     } else {
//       if (partition) {
//         size_t block_size = tuple_num / thread_num;
//         tmpkey = (block_size * thread_id) + (rnd.next() % block_size);
//       } else {
//         tmpkey = rnd.next() % tuple_num;
//       }
//     }

//     // decide operation type.
//     if ((rnd.next() % 100) < rratio) {
//       wonly_flag = false;
//       pro.emplace_back(Ope::READ, tmpkey);
//     } else {
//       ronly_flag = false;
//       if (rmw) {
//         pro.emplace_back(Ope::READ_MODIFY_WRITE, tmpkey);
//       } else {
//         pro.emplace_back(Ope::WRITE, tmpkey);
//       }
//     }
//   }

//   (*pro.begin()).ronly_ = ronly_flag;
//   (*pro.begin()).wonly_ = wonly_flag;

// #if KEY_SORT
//   std::sort(pro.begin(), pro.end());
// #endif // KEY_SORT

// #if ADD_ANALYSIS
//   res.local_make_procedure_latency_ += rdtscp() - start;
// #endif
// }

// inline static void makeProcedure(std::vector <Procedure> &pro,
//                                 Xoroshiro128Plus &rnd, size_t tuple_num,
//                                 size_t report_tuple_num, size_t max_ope,
//                                 [[maybe_unused]]Result &res) {
// #if ADD_ANALYSIS
//   uint64_t start = rdtscp();
// #endif
//   pro.clear();

//   size_t offset = tuple_num - report_tuple_num;
//   for (size_t i = 0; i < max_ope; ++i) {
//     uint64_t tmpkey;
//     // decide access destination key.
//     tmpkey = offset + rnd.next() % report_tuple_num;
//     pro.emplace_back(Ope::READ, tmpkey);
//   }

//   (*pro.begin()).ronly_ = true;

// #if KEY_SORT
//   std::sort(pro.begin(), pro.end());
// #endif // KEY_SORT

// #if ADD_ANALYSIS
//   res.local_make_procedure_latency_ += rdtscp() - start;
// #endif
// }

// inline static void makeBatchProcedure(std::vector <Procedure> &pro,
//                                 Xoroshiro128Plus &rnd, size_t tuple_num,
//                                 size_t report_tuple_num, size_t max_ope,
//                                 size_t rratio, bool rmw,
//                                 [[maybe_unused]]Result &res) {
// #if ADD_ANALYSIS
//   uint64_t start = rdtscp();
// #endif
//   pro.clear();
//   bool ronly_flag(true), wonly_flag(true);

//   size_t regular_ope = max_ope - report_tuple_num;
//   size_t report_ope = report_tuple_num;
//   if (regular_ope < 0)
//     regular_ope = 0;
//   if (report_ope > 0)
//     ronly_flag = false;

//   for (size_t i = 0; i < regular_ope; ++i) {
//     uint64_t tmpkey;
//     // decide access destination key.
//     tmpkey = rnd.next() % (tuple_num - report_tuple_num);

//     // decide operation type.
//     if ((rnd.next() % 100) < rratio) {
//       wonly_flag = false;
//       pro.emplace_back(Ope::READ, tmpkey);
//     } else {
//       ronly_flag = false;
//       if (rmw) {
//         pro.emplace_back(Ope::READ_MODIFY_WRITE, tmpkey);
//       } else {
//         pro.emplace_back(Ope::WRITE, tmpkey);
//       }
//     }
//   }

//   size_t offset = tuple_num - report_tuple_num;
//   for (size_t i = 0; i < report_ope; ++i) {
//     uint64_t tmpkey;
//     // decide access destination key.
//     tmpkey = offset + rnd.next() % report_tuple_num;
//     pro.emplace_back(Ope::WRITE, tmpkey);
//   }

//   (*pro.begin()).ronly_ = ronly_flag;
//   (*pro.begin()).wonly_ = wonly_flag;

// #if KEY_SORT
//   std::sort(pro.begin(), pro.end());
// #endif // KEY_SORT

// #if ADD_ANALYSIS
//   res.local_make_procedure_latency_ += rdtscp() - start;
// #endif
// }

[[maybe_unused]] inline static void sleepTics(size_t tics) {
  uint64_t start(rdtscp());
  while (rdtscp() - start < tics) _mm_pause();
}

template<typename Int>
Int byteswap(Int in) {
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
  }
}

template<typename Int>
void assign_as_bigendian(Int value, char *out) {
  Int tmp = byteswap(value);
  ::memcpy(out, &tmp, sizeof(tmp));
}

template<typename Int>
void parse_bigendian(const char *in, Int &out) {
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
inline std::size_t copy_cstr(char *out, const char *in, std::size_t out_buf_size) {
  if (out_buf_size == 0) return 0;
  std::size_t i = 0;
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
  for (uint8_t i : sv) {
    ::snprintf(buf, sizeof(buf), "%02x", i);
    ss << buf;
  }
  return ss.str();
}


template<typename T>
inline std::string_view str_view(const T &t) {
  return std::string_view(reinterpret_cast<const char *>(&t), sizeof(t));
}
