#pragma once

#include <atomic>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <array>
#include <xmmintrin.h>
#include <chrono>
#include <algorithm>

#include "../include/zipf.hh"

using namespace std;

class Result
{
public:
    uint64_t local_abort_counts_ = 0;
    uint64_t local_commit_counts_ = 0;
    uint64_t total_abort_counts_ = 0;
    uint64_t total_commit_counts_ = 0;
    uint64_t local_readphase_counts_ = 0;
    uint64_t local_writephase_counts_ = 0;
    uint64_t local_commitphase_counts_ = 0;
    uint64_t local_wwconflict_counts_ = 0;
    uint64_t total_readphase_counts_ = 0;
    uint64_t total_writephase_counts_ = 0;
    uint64_t total_commitphase_counts_ = 0;
    uint64_t total_wwconflict_counts_ = 0;
    uint64_t local_traversal_counts_ = 0;
    uint64_t total_traversal_counts_ = 0;
    uint64_t local_readonly_abort_counts_ = 0;
    uint64_t total_readonly_abort_counts_ = 0;
    vector<int> local_additionalabort;
    vector<int> total_additionalabort;

    void displayAllResult();

    void addLocalAllResult(const Result &other);
};

enum class Status : uint8_t
{
    inFlight,
    committed,
    aborted,
};

class Version
{
public:
    Version *prev_; // Pointer to overwritten version
    uint64_t val_;
    std::atomic<uint32_t> pstamp_; // Version access stamp, eta(V)
    std::atomic<uint32_t> sstamp_; // Version successor stamp, pi(V)
    std::atomic<uint32_t> cstamp_; // Version creation stamp, c(V)
    std::atomic<Status> status_;
    std::mutex mt_; // 各versionのlock

    Version() { init(); }

    void init();
};

class Tuple
{
public:
    uint64_t key;
    std::atomic<Version *> latest_;

    Tuple() { latest_.store(nullptr); }
};

enum class Ope : uint8_t
{
    READ,
    WRITE,
};

class Operation
{
public:
    uint64_t key_;
    uint64_t value_; // read value
    Version *ver_;

    Operation(uint64_t key, Version *ver, uint64_t value) : key_(key), ver_(ver), value_(value) {}
    Operation(uint64_t key, Version *ver) : key_(key), ver_(ver) {}
};

class Task
{
public:
    Ope ope_;
    uint64_t key_;
    uint64_t write_val_;

    Task(Ope ope, uint64_t key) : ope_(ope), key_(key) {}
    Task(Ope ope, uint64_t key, uint64_t write_val) : ope_(ope), key_(key), write_val_(write_val) {}
};

class Transaction
{
public:
    uint8_t thid_;                 // thread ID
    uint32_t cstamp_ = 0;          // Transaction end time, c(T)
    uint32_t pstamp_ = 0;          // Predecessor high-water mark, η (T)
    uint32_t sstamp_ = UINT32_MAX; // Successor low-water mark, pi (T)
    uint32_t txid_;                // TID and begin timestamp
    Status status_ = Status::inFlight;
    int abortcount_ = 0;

    vector<Operation> read_set_;  // write set
    vector<Operation> write_set_; // read set
    vector<Task> task_set_;       // 生成されたtransaction

    Result *res_;

    Transaction() {}

    Transaction(uint8_t thid, Result *res) : thid_(thid), res_(res)
    {
        read_set_.reserve(max_ope_readonly);
        write_set_.reserve(max_ope);
        task_set_.reserve(max_ope_readonly);
    }

    bool searchReadSet(unsigned int key);

    bool searchWriteSet(unsigned int key);

    void ssn_tbegin();

    void tbegin();

    void ssn_tread(Version *ver, uint64_t key);

    void tread(uint64_t key);

    void ssn_twrite(Version *desired, uint64_t key);

    void twrite(uint64_t key, uint64_t write_val);

    void ssn_commit();

    void commit();

    void ssn_abort();

    void abort();

    void verify_exclusion_or_abort();

    static Tuple *get_tuple(uint64_t key);

    bool isreadonly();
};

void print_mode();