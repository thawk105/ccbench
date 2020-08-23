/**
 * @file fileio.h
 * @brief File IO utilities.
 * @details This source is implemented by refering the source
 * https://github.com/starpos/oltp-cc-bench whose the author is Takashi Hoshino.
 * And Takayuki Tanabe revised.
 */

#pragma once

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <atomic>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>

#include "error.h"

#ifdef CCBENCH_LINUX

#include <linux/fs.h>

#endif  // CCBENCH_LINUX

namespace ccbench {

class File {  // NOLINT
public:
  File() = default;

  bool open(const std::string &filePath, int flags) {  // NOLINT
    fd_ = ::open(filePath.c_str(), flags);             // NOLINT
    autoClose_ = true;
    if (fd_ == -1) {
      throw LibcError(errno);  // NOLINT
    }
    return fd_ >= 0;
  }

  bool try_open(const std::string &file_path, int flags) {  // NOLINT
    fd_ = ::open(file_path.c_str(), flags);                 // NOLINT
    autoClose_ = true;
    return fd_ != -1;
  }

  bool open(const std::string &filePath, int flags, mode_t mode) {  // NOLINT
    fd_ = ::open(filePath.c_str(), flags, mode);                    // NOLINT
    autoClose_ = true;
    if (fd_ == -1) {
      throw LibcError(errno);  // NOLINT
    }
    return fd_ >= 0;
  }

  File(const std::string &filePath, int flags) : File() {
    if (!this->open(filePath, flags)) throwOpenError(filePath);
  }

  File(const std::string &filePath, int flags, mode_t mode) : File() {
    if (!this->open(filePath, flags, mode)) throwOpenError(filePath);
  }

  [[maybe_unused]] explicit File(int fd, bool autoClose)
          : fd_(fd), autoClose_(autoClose) {}

  ~File() noexcept try { close(); } catch (...) {
  }

  [[nodiscard]] int fd() const {  // NOLINT
    if (fd_ < 0) {
      std::cout << __FILE__ << " : " << __LINE__ << " : fatal error."
                << std::endl;
      std::abort();
    }
    return fd_;
  }

  void close() {
    if (!autoClose_ || fd_ < 0) return;
    if (::close(fd_) < 0) {
      throw LibcError(errno, "close failed: ");  // NOLINT
    }
    fd_ = -1;
  }

  [[maybe_unused]] void close_if_exist() {
    ::close(fd_);
    fd_ = -1;
  }

  /**
   * @brief read some data
   * @return the volume which is read.
   */
  size_t readsome(void *data, size_t size) const {  // NOLINT
    ssize_t r = ::read(fd(), data, size);
    if (r < 0) throw LibcError(errno, "read failed: ");  // NOLINT
    return r;
  }

  size_t read(void *data, size_t size) const {  // NOLINT
    char *buf = reinterpret_cast<char *>(data);  // NOLINT
    size_t s = 0;
    while (s < size) {
      size_t r = readsome(&buf[s], size - s);  // NOLINT
      s += r;
      if (size - s != r) return s;
    }
    return s;
  }

  void write(const void *data, size_t size) const {
    const char *buf = reinterpret_cast<const char *>(data);  // NOLINT
    size_t s = 0;
    while (s < size) {
      ssize_t r = ::write(fd(), &buf[s], size - s);         // NOLINT
      if (r < 0) throw LibcError(errno, "write failed: ");  // NOLINT
      if (r == 0) {
        std::cout << __FILE__ << " : " << __LINE__ << " : fatal error."
                  << std::endl;
        std::abort();
      }
      s += r;
    }
  }

#ifdef CCBENCH_LINUX

  [[maybe_unused]] void fdatasync() const {
    if (::fdatasync(fd()) < 0) {
      throw LibcError(errno, "fdsync failed: ");  // NOLINT
    }
  }

#endif  // CCBENCH_LINUX

  [[maybe_unused]] void fsync() const {
    if (::fsync(fd()) < 0) {
      throw LibcError(errno, "fsync failed: ");  // NOLINT
    }
  }

  [[maybe_unused]] void ftruncate(off_t length) const {
    if (::ftruncate(fd(), length) < 0) {
      throw LibcError(errno, "ftruncate failed: ");  // NOLINT
    }
  }

private:
  static void throwOpenError(const std::string &filePath) {
    const int err = errno;
    std::string s("open failed: ");  // NOLINT
    s += filePath;
    throw LibcError(err, s);
  }

  int fd_{-1};
  bool autoClose_{};
};

// create a file if it does not exist.
[[maybe_unused]] inline void createEmptyFile(const std::string &path,
                                             mode_t mode = 0644) {  // NOLINT
  struct stat st{};
  if (::stat(path.c_str(), &st) == 0) return;
  File writer(path, O_CREAT | O_TRUNC | O_RDWR, mode);  // NOLINT
  writer.close();
}

/**
 * Read all contents from a file.
 * Do not specify streams.
 * Do not use this for large files.
 *
 * String : it must have size(), resize(), and operator[].
 * such as std::string and std::vector<char>.
 */
template<typename String>
inline void readAllFromFile(File &file, String &buf) {
  constexpr const size_t usize = 4096;  // unit size.
  size_t rsize = buf.size();            // read data will be appended to buf.

  for (;;) {
    if (buf.size() < rsize + usize) buf.resize(rsize + usize);
    const size_t r = file.readsome(&buf[rsize], usize);
    if (r == 0) break;
    rsize += r;
  }
  buf.resize(rsize);
}

template<typename String>
[[maybe_unused]] inline void readAllFromFile(const std::string &path,
                                             String &buf) {
  File file(path, O_RDONLY);
  readAllFromFile(file, buf);
  file.close();
}

[[maybe_unused]] inline void genLogFileName(std::string &logpath,
                                            const int thid) {
  constexpr int PATHNAME_SIZE = 512;
  std::array<char, PATHNAME_SIZE> pathname{};

  memset(pathname.data(), '\0', PATHNAME_SIZE);

  if (getcwd(pathname.data(), PATHNAME_SIZE) == nullptr) {
    std::cout << __FILE__ << " : " << __LINE__ << " : fatal error."
              << std::endl;
    std::abort();
  }

  logpath = pathname.data();
  logpath += "/log/log" + std::to_string(thid);
}

}  // namespace ccbench
