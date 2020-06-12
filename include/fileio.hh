#pragma once

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <atomic>
#include <mutex>
#include <stdexcept>
#include <string>

#include "debug.hh"
#include "util.hh"

#ifdef Linux
#include <linux/fs.h>
#endif  // Linux

class File {
private:
  int fd_;
  bool autoClose_;

  void throwOpenError(const std::string &filePath) const {
    const int err = errno;
    std::string s("open failed: ");
    s += filePath;
    throw LibcError(err, s);
  }

public:
  File() : fd_(-1), autoClose_(false) {}

  bool open(const std::string &filePath, int flags) {
    fd_ = ::open(filePath.c_str(), flags);
    autoClose_ = true;
    return fd_ >= 0;
  }

  bool open(const std::string &filePath, int flags, mode_t mode) {
    fd_ = ::open(filePath.c_str(), flags, mode);
    autoClose_ = true;
    return fd_ >= 0;
  }

  File(const std::string &filePath, int flags) : File() {
    if (!this->open(filePath, flags)) throwOpenError(filePath);
  }

  File(const std::string &filePath, int flags, mode_t mode) : File() {
    if (!this->open(filePath, flags, mode)) throwOpenError(filePath);
  }

  explicit File(int fd, bool autoClose = false)
          : fd_(fd), autoClose_(autoClose) {}

  ~File() noexcept try { close(); } catch (...) {
  }

  int fd() const {
    if (fd_ < 0) ERR;
    return fd_;
  }

  void close() {
    if (!autoClose_ || fd_ < 0) return;
    if (::close(fd_) < 0) {
      throw LibcError(errno, "close failed: ");
    }
    fd_ = -1;
  }

  size_t readsome(void *data, size_t size) {
    ssize_t r = ::read(fd(), data, size);
    if (r < 0) throw LibcError(errno, "read failed: ");
    return r;
  }

  void read(void *data, size_t size) {
    char *buf = reinterpret_cast<char *>(data);
    size_t s = 0;
    while (s < size) {
      size_t r = readsome(&buf[s], size - s);
      if (r == 0) ERR;
      s += r;
    }
  }

  void write(const void *data, size_t size) {
    const char *buf = reinterpret_cast<const char *>(data);
    size_t s = 0;
    while (s < size) {
      ssize_t r = ::write(fd(), &buf[s], size - s);
      if (r < 0) throw LibcError(errno, "write failed: ");
      if (r == 0) ERR;
      s += r;
    }
  }

#ifdef Linux
  void fdatasync() {
    if (::fdatasync(fd()) < 0) {
      throw LibcError(errno, "fdsync failed: ");
    }
  }
#endif  // Linux

  void fsync() {
    if (::fsync(fd()) < 0) {
      throw LibcError(errno, "fsync failed: ");
    }
  }

  void ftruncate(off_t length) {
    if (::ftruncate(fd(), length) < 0) {
      throw LibcError(errno, "ftruncate failed: ");
    }
  }
};

// create a file if it does not exist.
inline void createEmptyFile(const std::string &path, mode_t mode = 0644) {
  struct stat st;
  if (::stat(path.c_str(), &st) == 0) return;
  File writer(path, O_CREAT | O_TRUNC | O_RDWR, mode);
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
inline void readAllFromFile(const std::string &path, String &buf) {
  File file(path, O_RDONLY);
  readAllFromFile(file, buf);
  file.close();
}

inline void genLogFileName(std::string &logpath, const int thid) {
  const int PATHNAME_SIZE = 512;
  char pathname[PATHNAME_SIZE];

  memset(pathname, '\0', PATHNAME_SIZE);

  if (getcwd(pathname, PATHNAME_SIZE) == NULL) ERR;

  logpath = pathname;
  logpath += "/log/log" + std::to_string(thid);
}
