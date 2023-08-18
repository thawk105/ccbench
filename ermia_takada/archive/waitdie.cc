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

/*#define TIDFLAG 1
#define CACHE_LINE_SIZE 64
#define PAGE_SIZE 4096
#define clocks_per_us 2100
#define extime 3
#define max_ope 10
#define max_ope_readonly 10
#define ronly_ratio 60
#define rratio 50
#define thread_num 10
#define tuple_num 10000
#define USE_LOCK 1*/

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
    uint64_t local_uselock_counts_ = 0;
    uint64_t total_uselock_counts_ = 0;
    uint64_t local_traversal_counts_ = 0;
    uint64_t total_traversal_counts_ = 0;

    void displayAllResult()
    {
        cout << "abort_counts_:\t\t\t" << total_abort_counts_ << endl;
        cout << "commit_counts_:\t\t\t" << total_commit_counts_ << endl;
        cout << "uselock_counts_:\t\t" << total_uselock_counts_ << endl;
        cout << "read SSNcheck abort:\t\t" << total_readphase_counts_ << endl;
        cout << "write SSNcheck abort:\t\t" << total_writephase_counts_ << endl;
        cout << "commit SSNcheck abort:\t\t" << total_commitphase_counts_ << endl;
        cout << "ww conflict abort:\t\t" << total_wwconflict_counts_ << endl;
        // displayAbortRate
        long double ave_rate =
            (double)total_abort_counts_ /
            (double)(total_commit_counts_ + total_abort_counts_);
        cout << fixed << setprecision(4) << "abort_rate:\t\t\t" << ave_rate << endl;
        cout << "traversal counts:\t\t" << total_traversal_counts_ << endl;
    }

    void addLocalAllResult(const Result &other)
    {
        total_abort_counts_ += other.local_abort_counts_;
        total_commit_counts_ += other.local_commit_counts_;
        total_uselock_counts_ += other.local_uselock_counts_;
        total_readphase_counts_ += other.local_readphase_counts_;
        total_writephase_counts_ += other.local_writephase_counts_;
        total_commitphase_counts_ += other.local_commitphase_counts_;
        total_wwconflict_counts_ += other.local_wwconflict_counts_;
        total_traversal_counts_ += other.local_traversal_counts_;
    }
};

bool isReady(const std::vector<char> &readys)
{
    for (const char &b : readys)
    {
        if (!b)
            return false;
    }
    return true;
}

void waitForReady(const std::vector<char> &readys)
{
    while (!isReady(readys))
    {
        _mm_pause();
    }
}

std::vector<Result> ErmiaResult;

void initResult() { ErmiaResult.resize(thread_num); }

std::atomic<uint64_t> timestampcounter(1); // timestampを割り当てる

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

    Version() { init(); }

    void init()
    {
        pstamp_.store(0, std::memory_order_release);
        sstamp_.store(UINT32_MAX, std::memory_order_release);
        cstamp_.store(0, std::memory_order_release);
        status_.store(Status::inFlight, std::memory_order_release);
        this->prev_ = nullptr;
    }
};

class Lock
{
public:
    std::atomic<int> counter;

    Lock() { counter.store(0, std::memory_order_release); }

    bool trylock()
    {
        int expected, desired(-1);
        expected = counter.load(std::memory_order_acquire);
        for (;;)
        {

            if (expected != 0)
            {
                return false;
            }
            if (counter.compare_exchange_weak(expected, desired, std::memory_order_acq_rel, std::memory_order_acquire))
                return true;
        }
    }

    void getlock()
    {
        int expected, desired(-1);
        for (;;)
        {
            expected = counter.load(memory_order_acquire);
            if (expected != 0)
                continue;
            if (counter.compare_exchange_weak(expected, -1, memory_order_acq_rel, memory_order_acquire))
                break;
        }
    }

    /*void lock_upgrade()
    {
        int expected, desired(1);
        for (;;)
        {
            expected = counter.load(memory_order_acquire);
        RETRY_U_LOCK:
            if (expected == 1)
                // if (expected != 0)
                continue;
            if (counter.compare_exchange_strong(expected, desired, memory_order_acq_rel, memory_order_acquire))
                break;
            else
                goto RETRY_U_LOCK;
        }
    }*/

    void unlock()
    {
        int expected, desired(0);
        expected = counter.load(memory_order_acquire);
        if (counter.compare_exchange_weak(expected, desired, memory_order_acq_rel, memory_order_acquire) == false)
        {
            cout << "error unlock" << endl;
        }
        // counter++;
    }
};

class Tuple
{
public:
    uint64_t key;
    std::atomic<Version *> latest_;
    Lock lock_;

    Tuple()
    {
        latest_.store(nullptr);
        Lock();
    }
};

Tuple *Table;       // databaseのtable
std::mutex SsnLock; // giant lock

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

void makeTask(std::vector<Task> &tasks, Xoroshiro128Plus &rnd, FastZipf &zipf)
{
    tasks.clear();
    if ((zipf() % 100) < ronly_ratio)
    {
        for (size_t i = 0; i < max_ope_readonly; ++i)
        {
            uint64_t tmpkey;
            // decide access destination key.
            tmpkey = zipf() % tuple_num;
            tasks.emplace_back(Ope::READ, tmpkey);
        }
    }
    else
    {
        for (size_t i = 0; i < (/*zipf() %*/ max_ope); ++i)
        {
            uint64_t tmpkey;
            // decide access destination key.
            tmpkey = zipf() % tuple_num;
            // decide operation type.
            if ((rnd.next() % 100) < rratio)
            {
                tasks.emplace_back(Ope::READ, tmpkey);
            }
            else
            {
                tasks.emplace_back(Ope::WRITE, tmpkey, zipf());
            }
        }
    }
}

class Transaction
{
public:
    uint8_t thid_;                 // thread ID
    uint32_t cstamp_ = 0;          // Transaction end time, c(T)
    uint32_t pstamp_ = 0;          // Predecessor high-water mark, η (T)
    uint32_t sstamp_ = UINT32_MAX; // Successor low-water mark, pi (T)
    uint32_t txid_;                // TID and begin timestamp
    bool lock_flag = false;
    bool deadlock_flag = false;
    Status status_ = Status::inFlight;

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

    bool searchReadSet(unsigned int key)
    {
        for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr)
        {
            if ((*itr).key_ == key)
                return true;
        }
        return false;
    }

    bool searchWriteSet(unsigned int key)
    {
        for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr)
        {
            if ((*itr).key_ == key)
                return true;
        }
        return false;
    }

    void tbegin()
    {
        // new transaction->get timestamp
        // if (this->lock_flag == false) // && this->deadlock_flag == false)
        this->txid_ = atomic_fetch_add(&timestampcounter, 1);
        this->cstamp_ = 0;
        pstamp_ = 0;
        sstamp_ = UINT32_MAX;
        status_ = Status::inFlight;
    }

    void ssn_tread(uint64_t key)
    {
        // read-own-writes, re-read from previous read in the same tx.
        if (searchWriteSet(key) == true || searchReadSet(key) == true)
            goto FINISH_TREAD;

        // get version to read
        Tuple *tuple;
        tuple = get_tuple(key);
        Version *ver;
        ver = tuple->latest_.load(memory_order_acquire);
        while (ver->status_.load(memory_order_acquire) != Status::committed || txid_ < ver->cstamp_.load(memory_order_acquire))
        {
            ver = ver->prev_;
            ++res_->local_traversal_counts_;
        }

        // update eta(t) with w:r edges
        this->pstamp_ = max(this->pstamp_, ver->cstamp_.load(memory_order_acquire));

        if (ver->sstamp_.load(memory_order_acquire) == (UINT32_MAX))
        { //// no overwrite yet
            read_set_.emplace_back(key, ver, ver->val_);
        }
        else
        {
            // update pi with r:w edge
            this->sstamp_ = min(this->sstamp_, ver->sstamp_.load(memory_order_acquire));
        }
        verify_exclusion_or_abort();
        if (this->status_ == Status::aborted)
        {
            ++res_->local_readphase_counts_;
            goto FINISH_TREAD;
        }
    FINISH_TREAD:
        return;
    }

    void ssn_twrite(uint64_t key, uint64_t write_val)
    {
        // update local write set
        if (searchWriteSet(key) == true)
            return;

        Tuple *tuple;
        tuple = get_tuple(key);

        // -------------------------------------------------------------------------------------
        // no wait
        if (tuple->lock_.trylock() == false)
        {
            this->status_ = Status::aborted;
            ++res_->local_wwconflict_counts_;
            return;
        }
        // -------------------------------------------------------------------------------------

        // If v not in t.writes:
        Version *expected, *desired;
        desired = new Version();
        desired->cstamp_ = this->txid_;
        desired->val_ = write_val;

        Version *vertmp;
        expected = tuple->latest_.load(memory_order_acquire);
        // -------------------------------------------------------------------------------------
        // wait  die
        /*while (tuple->lock_.trylock() == false)
        {
            expected = tuple->latest_.load(memory_order_acquire);
            if (expected->status_.load(memory_order_acquire) == Status::inFlight)
            {
                if (this->txid_ > expected->cstamp_.load(memory_order_acquire))
                {
                    this->status_ = Status::aborted;
                    ++res_->local_wwconflict_counts_;
                    this->deadlock_flag = true;
                    delete desired;
                    return;
                    // goto FINISH_TWRITE;
                }
                // expected = tuple->latest_.load(memory_order_acquire);
            }*/
        /*vertmp = expected;
        while (vertmp->status_.load(memory_order_acquire) != Status::committed)
        {
            vertmp = vertmp->prev_;
        }

        if (this->txid_ >= vertmp->cstamp_.load(memory_order_acquire))
        {
            this->status_ = Status::aborted;
            ++res_->local_wwconflict_counts_;
            this->deadlock_flag = true;
            // this->deadlock_flag = false;
            delete desired;
            return;
            // goto FINISH_TWRITE;
        }*/
        // abortしてlockを解放していない or read lockのせいでlockが取得できない
        // tuple->lock_.getlock();
        //}

        // -------------------------------------------------------------------------------------
        // expected = tuple->latest_.load(memory_order_acquire);
        desired->prev_ = expected;
        if (tuple->latest_.compare_exchange_weak(expected, desired, memory_order_acq_rel, memory_order_acquire) == false)
        {
            cout << "error compare " << tuple->lock_.counter.load(memory_order_acquire) << endl;
        }
        // tuple->latest_.store(desired, memory_order_release);

        // これがないとUpdate etaができない
        uint64_t tmpTID;
        tmpTID = thid_;
        tmpTID = tmpTID << 1;
        tmpTID |= 1;
        // cout << desired->prev_->pstamp_.load(memory_order_acquire) << endl;

        // desired->prev_->pstamp_.store(this->txid_, memory_order_release);

        // Update eta with w:r edge
        this->pstamp_ = max(this->pstamp_, desired->prev_->pstamp_.load(memory_order_acquire));

        // t.writes.add(V)
        write_set_.emplace_back(key, desired);

        // t.reads.discard(v)
        for (auto itr = read_set_.begin(); itr != read_set_.end();)
        {
            if (itr->key_ == key)
                itr = read_set_.erase(itr);
            else
                ++itr;
        }

        verify_exclusion_or_abort();
        if (this->status_ == Status::aborted)
        {
            // tuple->latest_.compare_exchange_weak(desired, expected, memory_order_acq_rel, memory_order_acquire);
            ++res_->local_writephase_counts_;
            goto FINISH_TWRITE;
        }

    FINISH_TWRITE:
        if (this->status_ == Status::aborted)
        {
            delete desired;
        }
        return;
    }

    void
    ssn_commit()
    {
        this->cstamp_ = atomic_fetch_add(&timestampcounter, 1);

        // begin pre-commit
        SsnLock.lock();

        // finalize pi(T)
        this->sstamp_ = min(this->sstamp_, this->cstamp_);
        for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr)
        {
            this->sstamp_ = min(this->sstamp_, (*itr).ver_->sstamp_.load(memory_order_acquire));
        }

        // finalize eta(T)
        for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr)
        {
            this->pstamp_ = max(this->pstamp_, (*itr).ver_->prev_->pstamp_.load(memory_order_acquire));
        }

        // ssn_check_exclusion
        if (pstamp_ < sstamp_)
            this->status_ = Status::committed;
        else
        {
            status_ = Status::aborted;
            ++res_->local_commitphase_counts_;
            SsnLock.unlock();
            return;
        }

        // update eta(V)
        for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr)
        {
            (*itr).ver_->pstamp_.store((max((*itr).ver_->pstamp_.load(memory_order_acquire), this->cstamp_)), memory_order_release);
        }

        // update pi
        for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr)
        {
            (*itr).ver_->prev_->sstamp_.store(this->sstamp_, memory_order_release);
            // initialize new version
            (*itr).ver_->pstamp_.store(this->cstamp_, memory_order_release);
            (*itr).ver_->cstamp_.store((*itr).ver_->pstamp_, memory_order_release);
        }

        // status, inFlight -> committed
        for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr)
        {
            (*itr).ver_->status_.store(Status::committed, memory_order_release);
            Tuple *tmp = get_tuple(itr->key_);
            tmp->lock_.unlock();
        }

        // -------------------------------------------------------------------------------------
        if (USE_LOCK == 1) // r-unlock
        {
            if (this->lock_flag == true)
            {
                this->lock_flag = false;
                for (auto itr = read_set_.begin(); itr != read_set_.end(); itr++)
                {
                    Tuple *tmp = get_tuple(itr->key_);
                    tmp->lock_.unlock();
                }
            }
        }
        // -------------------------------------------------------------------------------------
        this->status_ = Status::committed;
        this->deadlock_flag = false;
        SsnLock.unlock();
        read_set_.clear();
        write_set_.clear();
        return;
    }

    void abort()
    {
        for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr)
        {
            // Version *next_committed = (*itr).ver_->prev_;
            // while (next_committed->status_.load(memory_order_acquire) != Status::committed)
            //   next_committed = next_committed->prev_;
            //    cancel successor mark(sstamp)
            // next_committed->sstamp_.store(UINT32_MAX, memory_order_release);
            (*itr).ver_->status_.store(Status::aborted, memory_order_release);
            Tuple *tuple;
            tuple = get_tuple(itr->key_);
            tuple->lock_.unlock();
        }
        write_set_.clear();

        // notify that this transaction finishes reading the version now.
        read_set_.clear();
        ++res_->local_abort_counts_;
        if (lock_flag == true)
        {
            cout << "error" << endl;
        }

        // -------------------------------------------------------------------------------------
        if (USE_LOCK == 1 && isreadonly() == true)
        {
            // 提案手法: read only transactionのlock
            // dead lock(between read locks) prevention
            vector<int> tmp(task_set_.size());
            for (int i = 0; i < task_set_.size(); i++)
            {
                tmp.at(i) = task_set_.at(i).key_;
            }
            std::sort(tmp.begin(), tmp.end());
            tmp.erase(std::unique(tmp.begin(), tmp.end()), tmp.end());
            for (auto itr = tmp.begin(); itr != tmp.end(); itr++)
            {
                Tuple *tmptuple = get_tuple(*itr);
                // tmptuple->lock_.lock_upgrade();
                tmptuple->lock_.getlock();
            }
            this->txid_ = this->cstamp_;
            this->lock_flag = true;
            ++res_->local_uselock_counts_;
            return;
        }
        // -------------------------------------------------------------------------------------
    }

    void verify_exclusion_or_abort()
    {
        if (this->pstamp_ >= this->sstamp_)
        {
            this->status_ = Status::aborted;
        }
    }

    static Tuple *get_tuple(uint64_t key)
    {
        return (&Table[key]);
    }

    bool isreadonly()
    {
        for (auto itr = task_set_.begin(); itr != task_set_.end(); itr++)
        {
            if (itr->ope_ == Ope::WRITE)
            {
                return false;
            }
        }
        return true;
    }
};

void displayParameter()
{
    cout << "max_ope:\t\t\t" << max_ope << endl;
    cout << "rratio:\t\t\t\t" << rratio << endl;
    cout << "thread_num:\t\t\t" << thread_num << endl;
}

void makeDB()
{
    posix_memalign((void **)&Table, PAGE_SIZE, (tuple_num) * sizeof(Tuple));
    for (int i = 0; i < tuple_num; i++)
    {
        Table[i].key = 0;
        Version *verTmp = new Version();
        verTmp->status_.store(Status::committed, memory_order_release);
        verTmp->val_ = 0;
        Table[i].latest_.store(verTmp, memory_order_release);
    }
}

void worker(size_t thid, char &ready, const bool &start, const bool &quit)
{
    Xoroshiro128Plus rnd;
    rnd.init();
    Result &myres = std::ref(ErmiaResult[thid]);
    FastZipf zipf(&rnd, 0, tuple_num);

    Transaction trans(thid, (Result *)&ErmiaResult[thid]);

    ready = true;

    while (start == false)
    {
    }

    while (quit == false)
    {
        makeTask(trans.task_set_, rnd, zipf);

    RETRY:
        if (quit == true)
            break;

        trans.tbegin();
        for (auto itr = trans.task_set_.begin(); itr != trans.task_set_.end();
             ++itr)
        {
            if ((*itr).ope_ == Ope::READ)
            {
                trans.ssn_tread((*itr).key_);
            }
            else if ((*itr).ope_ == Ope::WRITE)
            {
                trans.ssn_twrite((*itr).key_, (*itr).write_val_);
            }
            // early abort.
            if (trans.status_ == Status::aborted)
            {
                trans.abort();

                goto RETRY;
            }
        }
        trans.ssn_commit();
        if (trans.status_ == Status::committed)
        {
            myres.local_commit_counts_++;
        }
        else if (trans.status_ == Status::aborted)
        {
            trans.abort();
            goto RETRY;
        }
    }
    return;
}

int main(int argc, char *argv[])
{
    displayParameter();
    makeDB();
    chrono::system_clock::time_point starttime, endtime;

    bool start = false;
    bool quit = false;
    initResult();
    std::vector<char> readys(thread_num);

    std::vector<std::thread> thv;
    for (size_t i = 0; i < thread_num; ++i)
    {
        thv.emplace_back(worker, i, std::ref(readys[i]), std::ref(start),
                         std::ref(quit));
    }
    waitForReady(readys);

    starttime = chrono::system_clock::now();
    start = true;
    this_thread::sleep_for(std::chrono::milliseconds(1000 * extime));
    quit = true;

    for (auto &th : thv)
    {
        th.join();
    }
    endtime = chrono::system_clock::now();

    double time = static_cast<double>(chrono::duration_cast<chrono::microseconds>(endtime - starttime).count() / 1000.0);
    // printf("time %lf[ms]\n", time);

    for (unsigned int i = 0; i < thread_num; ++i)
    {
        ErmiaResult[0].addLocalAllResult(ErmiaResult[i]);
    }
    ErmiaResult[0].displayAllResult();

    uint64_t result = (ErmiaResult[0].total_commit_counts_ * 1000) / time;
    cout << "latency[ns]:\t\t\t" << powl(10.0, 9.0) / result * thread_num
         << endl;
    cout << "throughput[tps]:\t\t" << result << endl;

    return 0;
}