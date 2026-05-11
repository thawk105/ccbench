#pragma once

#include <atomic>
#include <cstdint>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>

#include "../../include/cache_line_size.hh"
#include "tuple.hh"
#include "transaction_status.hh"
#include "txid.hh"

class Tuple;
typedef Tuple* Key;
#ifndef ORDERED_MAPSET
typedef std::map<Key,TxID> ReadSet;
typedef std::set<Key> WriteSet;
typedef std::set<Key> KeySet;
typedef std::set<TxID> TxSet;
#else
// Some test case will be fail due to the order of propagation?
typedef std::unordered_map<Key,TxID> ReadSet;
typedef std::unordered_set<Key> WriteSet;
typedef std::unordered_set<Key> KeySet;
typedef std::unordered_set<TxID> TxSet;
#endif

class TxNode {
    public:
        TxID id_;
        TransactionStatus status_;
        bool is_aborted;
        ReadSet readSet_;
        WriteSet writeSet_;
        TxSet readBy_;     // outgoing edges
        TxSet writtenBy_;  // outgoing edges
        TxSet from_;       // incoming edges
        TxNode(TxID id) :
            id_(id),
            is_aborted(false),
            status_(TransactionStatus::inflight)
            {};
        TxNode(TxID id, TransactionStatus status) :
            id_(id),
            is_aborted(false),
            status_(status)
            {};
};

#ifndef ORDERED_MAPSET
typedef std::map<TxID,TxNode> Graph;
#else
typedef std::unordered_map<TxID,TxNode> Graph;
#endif

class AbortTx {
    public:
        TxID id_;
        atomic <AbortTx*> next_;

        AbortTx(const TxID txid) {
            id_ = txid;
            next_.store(nullptr, memory_order_release);
        }

        AbortTx* ldAcqNext() {
            return next_.load(std::memory_order_acquire);
        }

        void strRelNext(AbortTx* next) {  // store release next = strRelNext
            next_.store(next, std::memory_order_release);
        }
};
