/**
 * @file tuple.h
 * @brief about tuple
 */
#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace ccbench {

class Tuple {  // NOLINT
public:
  Tuple() {}

  Tuple(std::string_view key, std::string_view val) : key_(key), val_(val) {}

  [[nodiscard]] std::string_view get_key() const { return key_; }

  [[nodiscard]] std::string_view get_value() const { return val_; }

  void set_value(std::string_view val) {
    val_ = val;
  }

private:
  std::string key_;
  std::string val_;
};

}  // namespace ccbench
