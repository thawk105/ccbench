/**
 * @file tuple_local.h
 * @brief header about Tuple::Impl
 */

#pragma once

#include <atomic>
#include <string>

#include "tuple.h"

namespace ccbench {

class Tuple::Impl {
public:
  Impl() {}  // NOLINT
  Impl(const char *key_ptr, std::size_t key_length, const char *value_ptr,
       std::size_t value_length);

  Impl(const Impl &right);

  Impl(Impl &&right);

  /**
   * @brief copy assign operator
   * @pre this is called by read_record function at xact.cc only .
   */
  Impl &operator=(const Impl &right);  // NOLINT

  Impl &operator=(Impl &&right);  // NOLINT

  ~Impl() {
    if (this->need_delete_pvalue_) {
      delete pvalue_.load(std::memory_order_acquire);  // NOLINT
    }
  }

  [[nodiscard]] std::string_view get_key() const;    // NOLINT
  [[nodiscard]] std::string_view get_value() const;  // NOLINT
  void reset();

  /**
   * @brief set key and value
   * @details Substitute argment for key_.
   * If a memory area has already been allocated for value, that area is
   * released. Allocates a new memory area for value and initializes it.
   * @param[in] key_ptr Pointer to key
   * @param[in] key_length Size of key
   * @param[in] value_ptr Pointer to value
   * @param[in] value_length Size of value
   * @return void
   */
  void set(const char *key_ptr, std::size_t key_length, const char *value_ptr,
           std::size_t value_length);

  /**
   * @brief set key of data in local
   * @details The memory area of old local data is overwritten..
   * @return void
   */
  [[maybe_unused]] void set_key(const char *key_ptr, std::size_t key_length);

  /**
   * @brief set value of data in local
   * @details The memory area of old local data is released immediately.
   * @return void
   */
  void set_value(const char *value_ptr, std::size_t value_length);

  /**
   * @brief Set value
   * @details Update value and preserve old value, so this
   * function is called by updater in write_phase.
   * @param[in] value_ptr Pointer to value
   * @param[in] value_length Size of value
   * @param[out] old_value To tell the caller the old value.
   * @return void
   */
  void set_value(const char *value_ptr, std::size_t value_length,
                 std::string **old_value);

private:
  std::string key_{};
  std::atomic<std::string *> pvalue_{nullptr};
  bool need_delete_pvalue_{false};
};

}  // namespace ccbench
