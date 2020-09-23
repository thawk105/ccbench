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

#include "heap_object.hpp"


namespace ccbench {

class Tuple {  // NOLINT
public:
  Tuple() = default;

  /**
   * @pre val_align must be larger than 20 to avoid SSO optimization of std::string.
   * @param key
   * @param val
   * @param val_align
   */
  Tuple(std::string_view key, std::string_view val, std::align_val_t val_align)
    : key_(key), val_() {
    val_.deep_copy_from(val.data(), val.size(), val_align);
  }

  Tuple(const Tuple &right) : Tuple() {
    key_ = right.key_;
    val_.deep_copy_from(right.val_);
  }

  Tuple(Tuple &&right) noexcept : Tuple() {
    key_ = std::move(right.key_);
    val_ = std::move(right.val_);
  }

  Tuple(std::string_view key, HeapObject&& obj) : Tuple() {
    set(key, std::move(obj));
  }

  Tuple &operator=(const Tuple &right) {
    key_ = right.key_;
    val_.deep_copy_from(right.val_);
    return *this;
  }

  Tuple &operator=(Tuple &&right) noexcept {
    key_ = std::move(right.key_);
    val_ = std::move(right.val_);
    return *this;
  }

  [[nodiscard]] std::string_view get_key() const { return key_; }

  [[nodiscard]] std::string_view get_val() const { return val_.view(); }

  [[nodiscard]] void *get_val_ptr() { return val_.data(); }

  [[nodiscard]] std::size_t get_val_size() const { return val_.size(); }

  [[nodiscard]] std::align_val_t get_val_align() const { return val_.align(); }

  void set_key(std::string_view key) {
    key_ = key; // copy
  }
  void set_value(HeapObject&& val) {
    val_ = std::move(val);
  }
  void set(std::string_view key, HeapObject&& val) {
    set_key(key);
    set_value(std::move(val));
  }
  void swap_value(HeapObject& val) noexcept { val_.swap(val); }
  void swap_value(Tuple& rhs) noexcept { swap_value(rhs.val_); }
  HeapObject& get_value() { return val_; }
  const HeapObject& get_value() const { return val_; }

  void set_value_shallow(const Tuple& rhs) {
    val_.shallow_copy_from(rhs.val_);
  }
  bool is_value_owned() const { return val_.is_owned(); }

private:
  std::string key_{};
  HeapObject val_;
};

}  // namespace ccbench
