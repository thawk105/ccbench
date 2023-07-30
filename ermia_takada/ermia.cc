#include <atomic>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <xmmintrin.h>

#include "../include/zipf.hh"

#define TIDFLAG 1
#define CACHE_LINE_SIZE 64
#define PAGE_SIZE 4096
#define clocks_per_us 2100 //"CPU_MHz. Use this info for measuring time."
#define extime 1           //Execution time[sec].
#define max_ope 100        //Total number of operations per single transaction."
#define rratio 50          //read ratio of single transaction.
#define thread_num 10      //Total number of worker threads.
#define tuple_num 10       //"Total number of records."


using namespace std;

class Result {
public:
    uint64_t local_abort_counts_ = 0;
    uint64_t local_commit_counts_ = 0;
    uint64_t total_abort_counts_ = 0;
    uint64_t total_commit_counts_ = 0;

    void displayAllResult() {
        cout << "abort_counts_:\t" << total_abort_counts_ << endl;
        cout << "commit_counts_:\t" << total_commit_counts_ << endl;
        //displayAbortRate
        long double ave_rate =
                (double) total_abort_counts_ /
                (double) (total_commit_counts_ + total_abort_counts_);
        cout << fixed << setprecision(4) << "abort_rate:\t" << ave_rate << endl;
        //displayTps
        uint64_t result = total_commit_counts_ / extime;
        cout << "latency[ns]:\t" << powl(10.0, 9.0) / result * thread_num
             << endl;
        cout << "throughput[tps]:\t" << result << endl;
    }

    void addLocalAllResult(const Result& other) {
        total_abort_counts_ += other.local_abort_counts_;
        total_commit_counts_ += other.local_commit_counts_;
    }
};

bool isReady(const std::vector<char>& readys) {
    for (const char& b : readys) {
        if (!b) return false;
    }
    return true;
}

void waitForReady(const std::vector<char>& readys) {
    while (!isReady(readys)) { _mm_pause(); }
}

std::vector<Result> ErmiaResult;

void initResult() { ErmiaResult.resize(thread_num); }


std::atomic<uint64_t> timestampcounter(0); //timestampを割り当てる

enum class VersionStatus : uint8_t {
    inFlight,
    committed,
    aborted,
};

class Version {
public:
    uint32_t pstamp_; // Version access stamp, eta(V),
    uint32_t sstamp_; // Version successor stamp, pi(V)
    Version* prev_;   // Pointer to overwritten version
    uint32_t cstamp_; // Version creation stamp, c(V)
    VersionStatus status_;
    uint64_t val_;

    Version() {}
};

class Tuple {
public:
    Version* latest_;
    uint64_t key;

    Tuple() {}
};

enum class TransactionStatus : uint8_t {
    inFlight,
    committing,
    committed,
    aborted,
};

Tuple* Table;       //databaseのtable
std::mutex SsnLock; //giant lock

enum class Ope : uint8_t {
    READ,
    WRITE,
};

class Operation {
public:
    uint64_t key_;
    Version* ver_;

    Operation(uint64_t key, Version* ver) : key_(key), ver_(ver) {}
};

class Task {
public:
    Ope ope_;
    uint64_t key_;
    uint64_t write_val_;

    Task(Ope ope, uint64_t key) : ope_(ope), key_(key) {}
    Task(Ope ope, uint64_t key, uint64_t write_val)
        : ope_(ope), key_(key), write_val_(write_val) {}
};

void makeTask(std::vector<Task>& tasks, Xoroshiro128Plus& rnd, FastZipf& zipf) {
    tasks.clear();
    for (size_t i = 0; i < max_ope; ++i) {
        uint64_t tmpkey;
        // decide access destination key.
        tmpkey = zipf() % tuple_num;
        // decide operation type.
        if ((rnd.next() % 100) < rratio) {
            tasks.emplace_back(Ope::READ, tmpkey);
        } else {
            tasks.emplace_back(Ope::WRITE, tmpkey, zipf());
        }
    }
}

class Transaction {
public:
    uint64_t write_val_;
    uint8_t thid_;                 // thread ID
    uint32_t cstamp_ = 0;          // Transaction end time, c(T)
    uint32_t pstamp_ = 0;          // Predecessor high-water mark, η (T)
    uint32_t sstamp_ = UINT32_MAX; // Successor low-water mark, pi (T)
    uint32_t txid_;                // TID and begin timestamp
    TransactionStatus status_ = TransactionStatus::inFlight;

    vector<Operation> read_set_;  //write set
    vector<Operation> write_set_; //read set
    vector<Task> task_set_;       //生成されたtransaction

    Result* eres_;

    Transaction() {}

    Transaction(uint8_t thid, Result* eres) : thid_(thid), eres_(eres) {
        read_set_.reserve(max_ope);
        write_set_.reserve(max_ope);
        task_set_.reserve(max_ope);
    }

    bool searchReadSet(unsigned int key) {
        for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
            if ((*itr).key_ == key) return true;
        }
        return false;
    }

    bool searchWriteSet(unsigned int key) {
        for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
            if ((*itr).key_ == key) return true;
        }
        return false;
    }

    void tbegin() {
        if (this->status_ == TransactionStatus::aborted) {
            this->txid_ = this->cstamp_;
        } else {
            this->txid_ = atomic_fetch_add(&timestampcounter, 1);
        }
        this->cstamp_ = 0;
        pstamp_ = 0;
        sstamp_ = UINT32_MAX;
        status_ = TransactionStatus::inFlight;
    }

    void ssn_tread(uint64_t key) {
        //read-own-writes, re-read from previous read in the same tx.
        if (searchWriteSet(key) == true || searchReadSet(key) == true)
            goto FINISH_TREAD;

        //get version to read
        Tuple* tuple;
        tuple = get_tuple(key);
        Version* ver;
        ver = tuple->latest_;
        while (ver->status_ != VersionStatus::committed || txid_ < ver->cstamp_)
            ver = ver->prev_;

        if (ver->sstamp_ == (UINT32_MAX)) { //// no overwrite yet
            /*Operation tmp;
            tmp.key_ = key;
            tmp.ver_ = ver;
            read_set_.emplace_back(tmp);*/
            read_set_.emplace_back(key, ver);
        } else {
            // update pi with r:w edge
            this->sstamp_ = min(this->sstamp_, ver->sstamp_);
        }

        verify_exclusion_or_abort();
        if (this->status_ == TransactionStatus::aborted) {
            //cout << "abort" << endl;
            goto FINISH_TREAD;
        }
    FINISH_TREAD:
        return;
    }

    void ssn_twrite(uint64_t key) {
        //update local write set.
        if (searchWriteSet(key) == true) return;

        Tuple* tuple;
        tuple = get_tuple(key);

        //If v not in t.writes:
        //first-updater-wins rule
        //Forbid a transaction to update a record that has a committed version later than its begin timestamp.
        Version *expected, *desired;
        desired = new Version();
        desired->cstamp_ = this->txid_;
        //desired->val_ = write_val;
        desired->status_ = VersionStatus::inFlight;
        desired->pstamp_ = 0;
        desired->sstamp_ = UINT32_MAX;

        Version* vertmp;
        expected = tuple->latest_;
        for (;;) {
            // w-w conflict : first updater wins rule
            if (expected->status_ == VersionStatus::inFlight) {
                if (this->txid_ <= expected->cstamp_) {
                    this->status_ = TransactionStatus::aborted;
                    //cout << "abort1" << endl;
                    goto FINISH_TWRITE;
                }
            }

            // if latest version is not comitted, vertmp is latest committed version.
            vertmp = expected;
            while (vertmp->status_ != VersionStatus::committed) {
                vertmp = vertmp->prev_;
            }

            if (txid_ < vertmp->cstamp_) {
                // w-w conflict, first-updater-wins rule.
                // Writers must abort if they would overwirte a version created after their snapshot.
                this->status_ = TransactionStatus::aborted;
                //cout << "abort2" << endl;
                goto FINISH_TWRITE;
            }

            desired->prev_ = expected;
            //tuple->latest_ = expected;
            //tuple->latest_ = desired;
            //if (tuple->latest_.compare_exchange_strong(expected, desired,memory_order_acq_rel,memory_order_acquire))
            if (tuple->latest_ == expected) {
                tuple->latest_ = desired;
                break;
            }
        }

        //Insert my tid for ver->prev_->sstamp_
        desired->prev_->sstamp_ = this->txid_;

        this->pstamp_ = max(this->pstamp_,
                            desired->prev_->pstamp_); //Update eta with w:r edge

        write_set_.emplace_back(key, desired); //t.writes.add(V)

        //これのせいでsegmentation faultになることがある(error)...
        //verify_exclusion_or_abort();

    FINISH_TWRITE:
        if (this->status_ == TransactionStatus::aborted) {
            delete desired;
            //tuple->latest_ = desired;
            //cout << "abort" << endl;
        }
        return;
    }

    void ssn_commit() {
        this->status_ = TransactionStatus::committing;
        this->cstamp_ = atomic_fetch_add(&timestampcounter, 1);

        // begin pre-commit
        SsnLock.lock();

        // finalize pi(T)
        this->sstamp_ = min(this->sstamp_, this->cstamp_);
        for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
            this->sstamp_ = min(this->sstamp_, (*itr).ver_->sstamp_);
        }

        // finalize eta(T)
        for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
            this->pstamp_ = max(this->pstamp_, (*itr).ver_->prev_->pstamp_);
        }

        // ssn_check_exclusion
        if (pstamp_ < sstamp_) this->status_ = TransactionStatus::committed;
        else {
            status_ = TransactionStatus::aborted;
            //cout << "abort3" << endl;
            SsnLock.unlock();
            return;
        }

        // update eta(V)
        for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
            (*itr).ver_->pstamp_ = (max((*itr).ver_->pstamp_, this->cstamp_));
        }

        // update pi
        for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
            (*itr).ver_->prev_->sstamp_ = this->sstamp_;
            // initialize new version
            (*itr).ver_->cstamp_ = (*itr).ver_->pstamp_ = this->cstamp_;
        }

        // status, inFlight -> committed
        for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
            (*itr).ver_->val_ = this->write_val_;
            (*itr).ver_->status_ = VersionStatus::committed;
        }

        this->status_ = TransactionStatus::committed;
        SsnLock.unlock();
        read_set_.clear();
        write_set_.clear();
        return;
    }


    void abort() {
        for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
            Version* next_committed = (*itr).ver_->prev_;
            while (next_committed->status_ != VersionStatus::committed)
                next_committed = next_committed->prev_;
            //cancel successor mark(sstamp).
            next_committed->sstamp_ = UINT32_MAX;
            (*itr).ver_->status_ = VersionStatus::aborted;
        }
        write_set_.clear();

        //notify that this transaction finishes reading the version now.
        read_set_.clear();
        ++eres_->local_abort_counts_;
    }

    void verify_exclusion_or_abort() {
        if (this->pstamp_ >= this->sstamp_) {
            this->status_ = TransactionStatus::aborted;
        }
    }

    static Tuple* get_tuple(/*Tuple* table,*/ uint64_t key) {
        return (&Table[key]);
    }
};

void displayParameter() {
    cout << "max_ope:\t\t\t\t" << max_ope << endl;
    cout << "rratio:\t\t\t\t" << rratio << endl;
    cout << "thread_num:\t\t\t" << thread_num << endl;
}

void displayDB() {
    Tuple* tuple;
    Version* version;

    for (int i = 0; i < tuple_num; ++i) {
        //for (auto itr = Table->begin(); itr != Table->end(); itr++) {
        tuple = &Table[i];
        cout << "------------------------------" << endl; // - 30
        cout << "key: " << i << endl;

        //version = tuple->latest_;
        version = tuple->latest_;

        while (version != NULL) {
            cout << "val: " << version->val_;

            switch (version->status_) {
                case VersionStatus::inFlight:
                    cout << " status:  inFlight/";
                    break;
                case VersionStatus::aborted:
                    cout << " status:  aborted/";
                    break;
                case VersionStatus::committed:
                    cout << " status:  committed/";
                    break;
            }
            //cout << endl;

            cout << " /cstamp:  " << version->cstamp_;
            cout << " /pstamp:  " << version->pstamp_;
            cout << " /sstamp:  " << version->sstamp_ << endl;
            //cout << endl;

            version = version->prev_;
        }
    }
}

void makeDB() {
    posix_memalign((void**) &Table, PAGE_SIZE, (tuple_num) * sizeof(Tuple));
    for (int i = 0; i < tuple_num; i++) {
        Table[i].key = 0;
        Version* verTmp = new Version;
        verTmp->cstamp_ = 0;
        verTmp->pstamp_ = 0;
        verTmp->sstamp_ = UINT32_MAX;
        verTmp->prev_ = nullptr;
        verTmp->status_ = VersionStatus::committed;
        //verTmp->readers_ = 0;
        verTmp->val_ = 0;
        Table[i].latest_ = verTmp;
    }
}

void worker(size_t thid, char& ready, const bool& start, const bool& quit) {
    Xoroshiro128Plus rnd;
    rnd.init();
    Result& myres = std::ref(ErmiaResult[thid]);
    FastZipf zipf(&rnd, 0, tuple_num);

    Transaction trans(thid, (Result*) &ErmiaResult[thid]);

    ready = true;

    while (start == false) {}

    while (quit == false) {
        makeTask(trans.task_set_, rnd, zipf);
        //cout << "task ok" << endl;

    RETRY:
        if (quit == true) break;

        trans.tbegin();
        for (auto itr = trans.task_set_.begin(); itr != trans.task_set_.end();
             ++itr) {
            if ((*itr).ope_ == Ope::READ) {
                //cout << "readしてる" << endl;
                trans.ssn_tread((*itr).key_);

            } else if ((*itr).ope_ == Ope::WRITE) {
                //cout << "writeしてる" << endl;
                trans.ssn_twrite((*itr).key_);
            }
            //early abort.
            if (trans.status_ == TransactionStatus::aborted) {
                //cout << "early abort" << thid << endl;
                trans.abort();

                goto RETRY;
            }
        }
        trans.ssn_commit();
        if (trans.status_ == TransactionStatus::committed) {
            myres.local_commit_counts_++;
            //cout << "commit ok" << trans.txid_ << endl;
            //std::cout << "txid_" << trans.txid_ << endl;
            //std::cout << "cstamp_" << trans.cstamp_ << endl;
        } else if (trans.status_ == TransactionStatus::aborted) {
            trans.abort();
            //cout << "normal abort" << thid << endl;
            goto RETRY;
        }
    }
    return;
}


int main(int argc, char* argv[]) {
    displayParameter();
    makeDB();
    cout << "make DB ok" << endl;

    //displayDB();

    bool start = false;
    bool quit = false;
    initResult();
    std::vector<char> readys(thread_num);

    std::vector<std::thread> thv;
    for (size_t i = 0; i < thread_num; ++i) {
        thv.emplace_back(worker, i, std::ref(readys[i]), std::ref(start),
                         std::ref(quit));
    }
    waitForReady(readys);
    start = true;
    this_thread::sleep_for(std::chrono::milliseconds(1000 * extime));
    quit = true;

    for (auto& th : thv) { th.join(); }

    cout << "thread join" << endl;

    for (unsigned int i = 0; i < thread_num; ++i) {
        ErmiaResult[0].addLocalAllResult(ErmiaResult[i]);
    }
    ErmiaResult[0].displayAllResult();

    //displayDB();

    return 0;
}