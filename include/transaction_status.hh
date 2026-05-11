#pragma once

#include <iostream>

enum class TransactionStatus : uint8_t {
  invalid,
  inflight,
  committing,
  committed,
  aborted,
  canceled,
};
