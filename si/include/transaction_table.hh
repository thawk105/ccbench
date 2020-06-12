#pragma once

class TransactionTable {
public:
  alignas(CACHE_LINE_SIZE) std::atomic <uint32_t> txid_;
  std::atomic <uint32_t> lastcstamp_;

  TransactionTable() {};

  TransactionTable(uint32_t txid, uint32_t lastcstamp) {
    this->txid_.store(txid, std::memory_order_relaxed);
    this->lastcstamp_.store(lastcstamp, std::memory_order_relaxed);
  }

  void set(uint32_t txid, uint32_t lastcstamp) {
    this->txid_.store(txid, std::memory_order_relaxed);
    this->lastcstamp_.store(lastcstamp, std::memory_order_relaxed);
  }
};
