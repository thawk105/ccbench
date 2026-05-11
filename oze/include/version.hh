#pragma once

#include <stdio.h>
#include <string.h>  // memcpy
#include <sys/time.h>

#include <atomic>
#include <cstdint>

#include "../../include/cache_line_size.hh"
#include "../../include/tuple_body.hh"

using namespace std;

enum class VersionStatus : uint8_t {
    invalid,
    pending,
    deleting,
    aborted,
    precommitted,
    committed,
    deleted,
    unused,
};

class Version {
public:
    TxID txid_;
    atomic <Version *> next_;
    atomic <VersionStatus> status_;

    TupleBody body_;

    Version() {
        status_.store(VersionStatus::pending, memory_order_release);
        next_.store(nullptr, memory_order_release);
    }

    Version(const TxID txid) {
        txid_ = txid;
        status_.store(VersionStatus::pending, memory_order_release);
        next_.store(nullptr, memory_order_release);
    }

    Version(const TxID txid, TupleBody&& body) : body_(body) {
        txid_ = txid;
        status_.store(VersionStatus::pending, memory_order_release);
        next_.store(nullptr, memory_order_release);
    }

    void set(const TxID txid, Version *next, const VersionStatus status) {
        txid_ = txid;
        status_.store(status, memory_order_release);
        next_.store(next, memory_order_release);
    }

    Version *ldAcqNext() { return next_.load(std::memory_order_acquire); }

    void strRelNext(Version *next) {  // store release next = strRelNext
        next_.store(next, std::memory_order_release);
    }
};
