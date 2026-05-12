#pragma once

#include <atomic>
#include <cstdint>

#include "../../include/cache_line_size.hh"
#include "version.hh"

class Tuple {
public:
  alignas(CACHE_LINE_SIZE) std::atomic<Version *> latest_;
  std::atomic <uint32_t> min_cstamp_;
  std::atomic <uint8_t> gc_lock_;
  TupleBody body_; // only used for index tuple as single version

  Tuple() {
    latest_.store(nullptr);
    gc_lock_.store(0, std::memory_order_release);
  }

  void init([[maybe_unused]] size_t thid, TupleBody&& body, void* param) {
    // for initializer
    min_cstamp_ = 0;
    latest_.store(new Version(), std::memory_order_release);
    Version *verTmp = latest_.load(std::memory_order_acquire);
    verTmp->cstamp_ = 0;
    // verTmp->pstamp = 0;
    // verTmp->sstamp = UINT64_MAX & ~(1);
    verTmp->psstamp_.pstamp_ = 0;
    verTmp->psstamp_.sstamp_ = UINT32_MAX & ~(1);
    // cstamp, sstamp の最下位ビットは TID フラグ
    // 1の時はTID, 0の時はstamp
    verTmp->prev_ = nullptr;
    verTmp->status_.store(VersionStatus::committed, std::memory_order_release);
    verTmp->readers_.store(0, std::memory_order_release);
    verTmp->body_ = std::move(body);
    body_ = std::ref(verTmp->body_);
  }

  void init(uint32_t txid, TupleBody&& body) {
    min_cstamp_ = 0;
    latest_.store(new Version(), std::memory_order_release);
    Version *verTmp = latest_.load(std::memory_order_acquire);
    verTmp->cstamp_.store(txid, memory_order_release);
    verTmp->psstamp_.pstamp_ = 0;
    verTmp->psstamp_.sstamp_ = UINT32_MAX & ~(1);
    verTmp->prev_ = nullptr;
    verTmp->body_ = std::move(body);
    body_ = std::ref(verTmp->body_);
  }
};
