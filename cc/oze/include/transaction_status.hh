#pragma once

#include <cstdint>

enum class TransactionStatus : uint8_t {
  invalid,
  inflight,
  committed,
  aborted,
  validating,
};
