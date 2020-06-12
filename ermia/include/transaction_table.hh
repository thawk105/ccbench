#pragma once

#include <atomic>
#include <cstdint>

#include "transaction_status.hh"

class TransactionTable {
public:
  alignas(CACHE_LINE_SIZE) std::atomic <uint32_t> txid_;
  std::atomic <uint32_t> cstamp_;
  std::atomic <uint32_t> sstamp_;
  std::atomic <uint32_t> lastcstamp_;
  std::atomic <TransactionStatus> status_;

  TransactionTable() {}

  TransactionTable(uint32_t txid, uint32_t cstamp, uint32_t sstamp,
                   uint32_t lastcstamp, TransactionStatus status) {
    this->txid_.store(txid, memory_order_relaxed);
    this->cstamp_.store(cstamp, memory_order_relaxed);
    this->sstamp_.store(sstamp, memory_order_relaxed);
    this->lastcstamp_.store(lastcstamp, memory_order_relaxed);
    this->status_.store(status, memory_order_relaxed);
  }

  void set(uint32_t txid, uint32_t cstamp, uint32_t sstamp, uint32_t lastcstamp,
           TransactionStatus status) {
    this->txid_.store(txid, memory_order_relaxed);
    this->cstamp_.store(cstamp, memory_order_relaxed);
    this->sstamp_.store(sstamp, memory_order_relaxed);
    this->lastcstamp_.store(lastcstamp, memory_order_relaxed);
    this->status_.store(status, memory_order_relaxed);
  }
};
