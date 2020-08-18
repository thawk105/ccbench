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
    if (val_.load(std::memory_order_acquire) != nullptr) {
      delete val_.load(std::memory_order_acquire);
    }
  }

  Tuple(std::string_view key, std::string_view val, std::size_t val_align) : key_(key), val_(new std::string(val)),
                                                                             val_align_(val_align) {}

  Tuple(const Tuple &right) {
    key_ = right.key_;
    val_.store(new std::string(*right.val_.load(std::memory_order_acquire)), std::memory_order_release);
  }

  Tuple(Tuple &&right) {
    key_ = std::move(right.key_);
    val_.store(right.val_.load(std::memory_order_acquire), std::memory_order_release);
    right.val_.store(nullptr, std::memory_order_release);
  }

  Tuple &operator=(const Tuple &right) {
    key_ = right.key_;
    val_.store(new std::string(*right.val_.load(std::memory_order_acquire)), std::memory_order_release);
    return *this;
  }

  Tuple &operator=(Tuple &&right) noexcept {
    key_ = std::move(right.key_);
    val_.store(right.val_.load(std::memory_order_acquire), std::memory_order_release);
    right.val_.store(nullptr, std::memory_order_release);
    return *this;
  }

  [[nodiscard]] std::string_view get_key() const { return key_; }

  [[nodiscard]] std::string_view get_val() const { return *val_.load(std::memory_order_acquire); }

  [[nodiscard]] std::size_t get_val_align() const { return val_align_; }

  void set_value(std::string_view val, std::size_t val_align) {
    delete val_.load(std::memory_order_acquire);
    val_.store(new std::string(val), std::memory_order_release);
    val_align_ = val_align;
  }

  void set_value(std::string_view val, std::string **old_val, std::size_t val_align) {
    *old_val = val_.load(std::memory_order_acquire);
    val_.store(new std::string(val), std::memory_order_release);
    val_align_ = val_align;
  }

  void set_val_align(std::size_t val_align) {
    val_align_ = val_align;
  }

private:
  std::string key_{};
  std::atomic<std::string *> val_{nullptr};
  std::size_t val_align_{};
};

}  // namespace ccbench
