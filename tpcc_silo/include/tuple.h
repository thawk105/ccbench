/**
 * @file tuple.h
 * @brief about tuple
 */
#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace ccbench {

class Tuple {  // NOLINT
public:
  Tuple() = default;

  ~Tuple() {
    if (val_ptr_.load(std::memory_order_acquire) != nullptr) {
      ::operator delete(val_ptr_.load(std::memory_order_acquire), get_val_size(), get_val_align());
    }
  }

  /**
   * @pre val_align must be larger than 20 to avoid SSO optimization of std::string.
   * @param key
   * @param val
   * @param val_align
   */
  Tuple(std::string_view key, std::string_view val, std::align_val_t val_align) : key_(key),
                                                                                  val_ptr_(::operator new(val.size(),
                                                                                                          val_align)),
                                                                                  val_size_(val.size()),
                                                                                  val_align_(val_align) {
    memcpy(get_val_ptr(), val.data(), val.size());
  }

  Tuple(const Tuple &right) {
    key_ = right.key_;
    set_val_ptr(::operator new(right.get_val_size(), right.get_val_align()));
    memcpy(get_val_ptr(), right.get_val_ptr(), right.get_val_size());
    set_val_size(right.get_val_size());
    set_val_align(right.get_val_align());
  }

  Tuple(Tuple &&right) noexcept {
    key_ = std::move(right.key_);
    set_val_ptr(right.get_val_ptr());
    set_val_size(right.get_val_size());
    set_val_align(right.get_val_align());
    right.set_val_ptr(nullptr);
  }

  Tuple &operator=(const Tuple &right) {
    key_ = right.key_;
    set_val_ptr(::operator new(right.get_val_size(), right.get_val_align()));
    memcpy(val_ptr_, right.get_val_ptr(), right.get_val_size());
    set_val_size(right.get_val_size());
    set_val_align(right.get_val_align());
    return *this;
  }

  Tuple &operator=(Tuple &&right) noexcept {
    key_ = std::move(right.key_);
    set_val_ptr(right.get_val_ptr());
    set_val_size(right.get_val_size());
    set_val_align(right.get_val_align());
    right.set_val_ptr(nullptr);
    return *this;
  }

  [[nodiscard]] std::string_view get_key() const { return key_; }

  [[nodiscard]] std::string_view get_val() const {
    return {reinterpret_cast<char *>(val_ptr_.load(std::memory_order_acquire)), get_val_size()};
  }

  [[nodiscard]] void *get_val_ptr() const { return val_ptr_.load(std::memory_order_acquire); }

  [[nodiscard]] std::size_t get_val_size() const { return val_size_; }

  [[nodiscard]] std::align_val_t get_val_align() const { return val_align_; }

  void set_key(std::string_view key) {
    this->key_ = key;
  }

  void set_value(std::string_view val, std::align_val_t val_align) {
    ::operator delete(val_ptr_.load(std::memory_order_acquire), val_size_, val_align_);
    set_val_ptr(::operator new(val.size(), val_align));
    memcpy(val_ptr_, val.data(), val.size());
    set_val_size(val.size());
    set_val_align(val_align);
  }

  std::tuple<void *, std::size_t, std::align_val_t>
  set_value_get_old_val(std::string_view val, std::align_val_t val_align) {
    void *old_val = val_ptr_.load(std::memory_order_acquire);
    std::size_t old_size = get_val_size();
    std::align_val_t old_align = get_val_align();
    set_val_ptr(::operator new(val.size(), val_align));
    memcpy(val_ptr_, val.data(), val.size());
    set_val_size(val.size());
    set_val_align(val_align);
    return std::make_tuple(old_val, old_size, old_align);
  }

  void set_val_align(std::align_val_t val_align) {
    val_align_ = val_align;
  }

  void set_val_ptr(void *ptr) {
    val_ptr_.store(ptr, std::memory_order_release);
  }

  void set_val_size(std::size_t val_size) {
    val_size_ = val_size;
  }

private:
  std::string key_{};
  std::atomic<void *> val_ptr_{nullptr};
  std::size_t val_size_{};
  std::align_val_t val_align_{};
};

}  // namespace ccbench
