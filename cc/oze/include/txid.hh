#pragma once

#include <iostream>

struct TxID {
    union {
        uint64_t obj_;
        struct {
            uint64_t tid: 24;
            uint64_t thid: 8;
            uint64_t epoch: 32;
        };
    };

    TxID() : obj_(0) {};
    bool operator==(const TxID &right) const { return obj_ == right.obj_; }
    bool operator!=(const TxID &right) const { return !operator==(right); }
    bool operator<(const TxID &right) const { return obj_ < right.obj_; }
    bool operator<=(const TxID &right) const { return obj_ <= right.obj_; }
    bool operator>(const TxID &right) const { return obj_ > right.obj_; }
    bool operator>=(const TxID &right) const { return obj_ >= right.obj_; }
    friend std::ostream& operator<<(std::ostream& left, const TxID &right) {
        left << right.epoch << ":" << right.thid << ":" << right.tid;
             // << right.obj_
             // << "(epoch=" << right.epoch
             // << ",thid=" << right.thid
             // << ",id=" << right.tid << ")";
        return left;
    }
};

namespace std {
template <> struct hash<TxID> {
    inline size_t operator()(const TxID &v) const {
        hash<uint64_t> hasher;
        return hasher(v.obj_);
    }
};
}
