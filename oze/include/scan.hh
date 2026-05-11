#pragma once

#include <stdio.h>

#include <atomic>
#include <cstdint>

#include "txid.hh"

class ScanEntry {
public:
  TxID txid_;
  std::string_view left_key_;
  std::string_view right_key_; 
  bool l_exclusive_;
  bool r_exclusive_;
  std::atomic<ScanEntry*> next_;

  ScanEntry(const TxID txid,
            std::string_view left_key, bool l_exclusive,
            std::string_view right_key, bool r_exclusive, ScanEntry*next) {
    txid_ = txid;
    left_key_ = left_key;
    right_key_ = right_key; 
    l_exclusive_ = l_exclusive;
    r_exclusive_ = r_exclusive;
    next_.store(next, std::memory_order_release);
  }

  ScanEntry *ldAcqNext() { return next_.load(std::memory_order_acquire); }

  void strRelNext(ScanEntry *next) {  // store release next = strRelNext
    next_.store(next, std::memory_order_release);
  }

  bool contains(std::string_view key) {
    if (left_key_.empty() && right_key_.empty()) {
      return true; // full scan
    }

    int left = key.compare(left_key_);   // > 0 if left_key < key
    int right = key.compare(right_key_); // < 0 if key < right_key

    if (left_key_.empty()) {
      return r_exclusive_ ? right < 0 : right <= 0;
    }

    if (right_key_.empty()) {
      return l_exclusive_ ? left > 0 : left >= 0;
    }

    if (l_exclusive_ && r_exclusive_) {
      return right < 0 && left > 0;
    }

    if (l_exclusive_) {
      return right <= 0 && left > 0;
    }

    if (r_exclusive_) {
      return right < 0 && left >= 0;
    }

    return right <= 0 && left >= 0;
  }
};
