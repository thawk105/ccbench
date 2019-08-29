#pragma once

#include <cstdint>

enum class TransactionStatus : uint8_t {
  inFlight,
  committing,
  committed,
  aborted,
};
