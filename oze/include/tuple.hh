#pragma once

#include <atomic>
#include <cstdint>
#include <map>

#include "../../include/cache_line_size.hh"

#include "oze.hh"
#include "version.hh"

// TODO: should use nullptr to represent "version does not exist"
// The nullptr is currenlty used for no readable version in serializablity perspective
// Note that there exists "no readable version" case even if the version is not nullptr,
// so it also must be fixed...
#define VERSION_TERMINATION (Version*)0x1

using namespace std;

struct TupleId {
  union {
    uint64_t obj_;
    struct {
      uint64_t tid: 32;
      uint64_t epoch: 32;
    };
  };

  TupleId() { obj_ = 0; }

  bool operator==(const TupleId &right) const { return obj_ == right.obj_; }

  bool operator!=(const TupleId &right) const { return !operator==(right); }

  bool operator<(const TupleId &right) const { return this->obj_ < right.obj_; }
};

class Tuple {
public:
  alignas(CACHE_LINE_SIZE) TupleId tuple_id_;
  RWLock lock_;
  atomic<Version *> latest_;
  TupleBody body_; // only used for index tuple as single version

  Graph graph_;
  std::map<uint64_t,std::vector<Version*>> version_index_;

  Tuple() : latest_(nullptr) {}

  Version *ldAcqLatest() { return latest_.load(std::memory_order_acquire); }

  void init(TxID txid, TupleBody&& body) {

    latest_.store(new Version(), std::memory_order_release);
    (latest_.load(std::memory_order_acquire))
            ->set(txid, VERSION_TERMINATION, VersionStatus::pending);
    (latest_.load(std::memory_order_acquire))->body_ = std::move(body);
    body_ = std::ref((latest_.load(std::memory_order_acquire))->body_);

    Graph* g = ::new(&graph_) Graph;
    g->emplace(txid, TxNode(txid, TransactionStatus::validating));
    version_index_ = std::map<uint64_t,std::vector<Version*>>();
  }

  // only for initial data preparation
  void init([[maybe_unused]] size_t thid, TupleBody&& body, void* param) {

    TxID txid = TxID();
    txid.thid = thid;
    latest_.store(new Version(), std::memory_order_release);
    (latest_.load(std::memory_order_acquire))
            ->set(txid, nullptr, VersionStatus::committed);
    (latest_.load(std::memory_order_acquire))->body_ = std::move(body);
    body_ = std::ref((latest_.load(std::memory_order_acquire))->body_);

    Graph* g = ::new(&graph_) Graph;
    g->emplace(txid, TxNode(txid, TransactionStatus::validating));
    version_index_ = std::map<uint64_t,std::vector<Version*>>();

    // Omit to add all the key/pages write set of this TxNode.
    // But probably no problem because no transaction can precede this tx.
  }
};
