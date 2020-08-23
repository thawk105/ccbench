/**
 * @file error.h
 * @brief error utilities.
 * @details This source is implemented by referring the source
 * https://github.com/starpos/oltp-cc-bench whose the author is Takashi Hoshino.
 * And Takayuki Tanabe revised.
 */

#pragma once

#include <array>
#include <cstring>
#include <exception>

namespace ccbench {

class LibcError : public std::exception {
private:
  std::string str_;

  static std::string generateMessage(int errnum,  // NOLINT
                                     const std::string &msg) {
    std::string s(msg);
    constexpr std::size_t BUF_SIZE = 1024;
    std::array<char, BUF_SIZE> buf{};
    ::snprintf(buf.data(), 1024, " %d ", errnum);  // NOLINT
    s += buf.data();
    if (::strerror_r(errnum, buf.data(), BUF_SIZE) != nullptr) {
      s += buf.data();
    }
    return s;
  }

public:
  explicit LibcError(int err_num = errno,                     // NOLINT
                     const std::string &msg = "libc_error:")  // NOLINT
          : str_(generateMessage(err_num, msg)) {}
};

} // namespace ccbench
