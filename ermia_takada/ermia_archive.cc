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
#define extime 3           //Execution time[sec].
#define max_ope 5          //Total number of operations per single transaction."
#define rratio 50          //read ratio of single transaction.
#define thread_num 1       //Total number of worker threads.
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
    uint32_t pstamp_;  // Version access stamp, eta(V),
    uint32_t sstamp_;  // Version successor stamp, pi(V)
    Version* prev_;    // Pointer to overwritten version
    uint64_t readers_; // summarize all of V's readers.
    uint32_t cstamp_;  // Version creation stamp, c(V)
    VersionStatus status_;
    uint64_t val_;

    void init() {
        pstamp_ = 0;
        sstamp_ = UINT32_MAX & ~(TIDFLAG);
        status_ = VersionStatus::inFlight;
        readers_ = 0;
    }
};

class Tuple {
public:
    Version* latest_;
    uint32_t min_cstamp_;

    Tuple() { latest_ = nullptr; }
};

enum class TransactionStatus : uint8_t {
    inFlight,
    committing,
    committed,
    aborted,
};

class TransactionTable {
public:
    uint32_t txid_;
    uint32_t cstamp_;
    uint32_t sstamp_;
    uint32_t lastcstamp_;
    TransactionStatus status_;

    TransactionTable(uint32_t txid, uint32_t cstamp, uint32_t sstamp,
                     uint32_t lastcstamp, TransactionStatus status) {
        this->txid_ = txid;
        this->cstamp_ = cstamp;
        this->sstamp_ = sstamp;
        this->lastcstamp_ = lastcstamp;
        this->status_ = status;
    }
};

Tuple* Table;           //databaseのtable
TransactionTable** TMT; // Transaction Mapping Table
std::mutex SsnLock;
//std::atomic<uint64_t> Lsn(0);


enum class Ope : uint8_t {
    READ,
    WRITE,
};

template<typename T>
class OpElement {
public:
    uint64_t key_;
    T* rcdptr_;

    OpElement(uint64_t key, T* rcdptr) : key_(key), rcdptr_(rcdptr) {}
};


template<typename T>
class SetElement : public OpElement<T> {
public:
    uint64_t key_;
    T* rcdptr_;
    Version* ver_;

    SetElement(uint64_t key, T* rcdptr, Version* ver)
        : OpElement<T>::OpElement(key, rcdptr) {
        this->ver_ = ver;
    }
};

class Task {
public:
    Ope ope_;
    uint64_t key_;
    bool ronly_ = false;
    bool wonly_ = false;

    Task(Ope ope, uint64_t key) : ope_(ope), key_(key) {}
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
            tasks.emplace_back(Ope::WRITE, tmpkey);
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
    uint32_t txid_;                // TID and begin timestamp - the current LSN
    uint32_t startstamp;

    vector<SetElement<Tuple>> read_set_;
    vector<SetElement<Tuple>> write_set_;
    vector<Task> task_set_; //生成されたtransaction

    Result* eres_;
    TransactionStatus status_ = TransactionStatus::inFlight;

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
        TransactionTable *newElement, *tmt;

        tmt = TMT[thid_];
        uint32_t lastcstamp;
        if (this->status_ == TransactionStatus::aborted) {
            //If this transaction is retry by abort, its lastcstamp is last one.
            lastcstamp = this->txid_ = tmt->lastcstamp_;
        } else {
            //If this transaction is after committed transaction, its lastcstamp is that's one.
            lastcstamp = this->txid_ = cstamp_;
        }
        newElement = new TransactionTable(0, 0, UINT32_MAX, lastcstamp,
                                          TransactionStatus::inFlight);

        //Check the latest commit timestamp
        for (unsigned int i = 0; i < thread_num; ++i) {
            tmt = TMT[i];
            this->txid_ = max(this->txid_, tmt->lastcstamp_);
        }
        //this->txid_++;
        this->txid_ = atomic_fetch_add(&timestampcounter, 1);
        newElement->txid_ = this->txid_;
        //New object is registerd to transaction mapping table.
        TMT[thid_] = newElement;
        pstamp_ = 0;
        sstamp_ = UINT32_MAX;
        status_ = TransactionStatus::inFlight;
        startstamp = atomic_fetch_add(&timestampcounter, 1);
    }

    void ssn_tread(uint64_t key) {
        //read-own-writes, re-read from previous read in the same tx.
        if (searchWriteSet(key) == true || searchReadSet(key) == true)
            goto FINISH_TREAD;
        //Search versions from data structure.
        Tuple* tuple;
        tuple = get_tuple(Table, key);
        //Move to the points of this view.
        Version* ver;
        ver = tuple->latest_;
        while (ver->status_ != VersionStatus::committed || txid_ < ver->cstamp_)
            ver = ver->prev_;

        if (ver->sstamp_ == (UINT32_MAX & ~(TIDFLAG))) {
            // no overwrite yet
            read_set_.emplace_back(key, tuple, ver);
        } else {
            // update pi with r:w edge
            this->sstamp_ = min(this->sstamp_, (ver->sstamp_ >> TIDFLAG));
        }
        //upReadersBits(ver);

        verify_exclusion_or_abort();
        if (this->status_ == TransactionStatus::aborted) { goto FINISH_TREAD; }
    FINISH_TREAD:
        return;
    }

    void ssn_twrite(uint64_t key) {
        //update local write set.
        if (searchWriteSet(key) == true) goto FINISH_TWRITE;
        //if (searchWriteSet2(key)==true) goto FINISH_TWRITE;

        //avoid false positive.
        Tuple* tuple;
        tuple = nullptr;

        //Search tuple from data structure.
        if (!tuple) { tuple = get_tuple(Table, key); }

        //If v not in t.writes: first-updater-wins rule
        //Forbid a transaction to update  a record that has a committed head version later than its begin timestamp.
        Version *expected, *desired;
        desired = new Version();
        desired->cstamp_ =
                this->txid_; // read operation, write operation, it is also accessed by garbage collection.

        Version* vertmp;
        expected = tuple->latest_;
        for (;;) {
            // w-w conflict : first updater wins rule
            if (expected->status_ == VersionStatus::inFlight) {
                if (this->txid_ <= expected->cstamp_) {
                    this->status_ = TransactionStatus::aborted;
                    TMT[thid_]->status_ = TransactionStatus::aborted;
                    goto FINISH_TWRITE;
                }
                expected = tuple->latest_;
                continue;
            }

            // if latest version is not comitted.
            vertmp = expected;
            while (vertmp->status_ != VersionStatus::committed) {
                vertmp = vertmp->prev_;
            }

            // vertmp is latest committed version.
            if (txid_ < vertmp->cstamp_) {
                //  write - write conflict, first-updater-wins rule.
                // Writers must abort if they would overwirte a version created after their snapshot.
                this->status_ = TransactionStatus::aborted;
                TMT[thid_]->status_ = TransactionStatus::aborted;
                goto FINISH_TWRITE;
            }

            desired->prev_ = expected;
            if (tuple->latest_ == expected) break;
        }

        //Insert my tid for ver->prev_->sstamp_
        uint64_t tmpTID;
        tmpTID = thid_;
        tmpTID = tmpTID << 1;
        tmpTID |= 1;
        desired->prev_->sstamp_ = tmpTID;

        //Update eta with w:r edge
        this->pstamp_ = max(this->pstamp_, desired->prev_->pstamp_);
        write_set_.emplace_back(key, tuple, desired);

        verify_exclusion_or_abort();

        //tuple->latest_ = expected;

    FINISH_TWRITE:
        return;
    }

    void ssn_commit() {
        this->status_ = TransactionStatus::committing;
        TransactionTable* tmt = TMT[thid_];
        tmt->status_ = TransactionStatus::committing;

        this->cstamp_ = atomic_fetch_add(&timestampcounter, 1);
        //this->cstamp_ = ++Lsn;
        tmt->cstamp_ = this->cstamp_;

        // begin pre-commit
        SsnLock.lock();

        // finalize eta(T)
        for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
            pstamp_ = max(pstamp_, (*itr).ver_->prev_->pstamp_);
        }

        // finalize pi(T)
        sstamp_ = min(sstamp_, cstamp_);
        for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
            uint32_t verSstamp = (*itr).ver_->sstamp_;
            // if the lowest bit raise, the record was overwrited by other concurrent transactions.
            //but in serial SSN, the validation of the concurrent transactions will be done after that of this transaction. So it can skip.
            // And if the lowest bit raise, the stamp is TID;
            if (verSstamp & 1) continue;
            sstamp_ = min(sstamp_, verSstamp >> 1);
        }

        // ssn_check_exclusion
        if (pstamp_ < sstamp_) status_ = TransactionStatus::committed;
        else {
            status_ = TransactionStatus::aborted;
            SsnLock.unlock();
            return;
        }

        // update eta
        for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
            (*itr).ver_->pstamp_ = (max((*itr).ver_->pstamp_, cstamp_));
            // down readers bit
            downReadersBits((*itr).ver_);
        }

        // update pi
        uint64_t verSstamp = this->sstamp_;
        verSstamp = verSstamp << 1;
        verSstamp &= ~(1);

        uint64_t verCstamp = cstamp_;
        verCstamp = verCstamp << 1;
        verCstamp &= ~(1);

        for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
            (*itr).ver_->prev_->sstamp_ = verSstamp;
            // initialize new version
            (*itr).ver_->pstamp_ = cstamp_;
            (*itr).ver_->cstamp_ = verCstamp;
        }

        // status, inFlight -> committed
        for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
            (*itr).ver_->val_ = write_val_;
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
            next_committed->sstamp_ = UINT32_MAX & ~(TIDFLAG);
            (*itr).ver_->status_ = VersionStatus::aborted;
        }
        write_set_.clear();

        //notify that this transaction finishes reading the version now.
        for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
            downReadersBits((*itr).ver_);
        }
        read_set_.clear();
        ++eres_->local_abort_counts_;
    }

    void verify_exclusion_or_abort() {
        if (this->pstamp_ >= this->sstamp_) {
            this->status_ = TransactionStatus::aborted;
            TransactionTable* tmt = TMT[thid_];
            tmt->status_ = TransactionStatus::aborted;
        }
    }

    void upReadersBits(Version* ver) {
        uint64_t expected, desired;
        expected = ver->readers_;
        for (;;) {
            desired = expected | (1 << thid_);
            //if (ver->readers_.compare_exchange_weak(expected, desired, memory_order_acq_rel, memory_order_acquire))
            if (ver->readers_ == expected) break;
        }
    }

    void downReadersBits(Version* ver) {
        uint64_t expected, desired;
        expected = ver->readers_;
        for (;;) {
            desired = expected & ~(1 << thid_);
            //if (ver->readers_.compare_exchange_weak(expected, desired, memory_order_acq_rel, memory_order_acquire))
            if (ver->readers_ == expected) break;
        }
    }

    static Tuple* get_tuple(Tuple* table, uint64_t key) { return &table[key]; }
};

void displayParameter() {
    cout << "#FLAGS_max_ope:\t\t\t\t" << max_ope << endl;
    cout << "#FLAGS_rratio:\t\t\t\t" << rratio << endl;
    cout << "#FLAGS_thread_num:\t\t\t" << thread_num << endl;
}

void chkArg() {
    displayParameter();

    TMT = new TransactionTable*[thread_num];

    for (unsigned int i = 0; i < thread_num; ++i) {
        TMT[i] = new TransactionTable(0, 0, UINT32_MAX, 0,
                                      TransactionStatus::inFlight);
    }
}

void displayDB() {
    Tuple* tuple;
    Version* version;

    for (unsigned int i = 0; i < tuple_num; ++i) {
        tuple = &Table[i];
        cout << "------------------------------" << endl; // - 30
        cout << "key: " << i << endl;

        version = tuple->latest_;
        while (version != NULL) {
            cout << "val: " << version->val_ << endl;

            switch (version->status_) {
                case VersionStatus::inFlight:
                    cout << "status:  inFlight";
                    break;
                case VersionStatus::aborted:
                    cout << "status:  aborted";
                    break;
                case VersionStatus::committed:
                    cout << "status:  committed";
                    break;
            }
            cout << endl;

            cout << "cstamp:  " << version->cstamp_ << endl;
            cout << "pstamp:  " << version->pstamp_ << endl;
            cout << "sstamp:  " << version->sstamp_ << endl;
            cout << endl;

            version = version->prev_;
        }
        cout << endl;
    }
}

void partTableInit([[maybe_unused]] size_t thid, uint64_t start, uint64_t end) {

    for (uint64_t i = start; i <= end; ++i) {
        Tuple* tmp;
        tmp = &Table[i];
        tmp->min_cstamp_ = 0;
        posix_memalign((void**) &tmp->latest_, CACHE_LINE_SIZE,
                       sizeof(Version));
        Version* verTmp = tmp->latest_;
        verTmp->cstamp_ = 0;
        // verTmp->pstamp = 0;
        // verTmp->sstamp = UINT64_MAX & ~(1);
        verTmp->pstamp_ = 0;
        verTmp->sstamp_ = UINT32_MAX & ~(1);
        // cstamp, sstamp の最下位ビットは TID フラグ
        // 1の時はTID, 0の時はstamp
        verTmp->prev_ = nullptr;
        verTmp->status_ = VersionStatus::committed;
        verTmp->readers_ = 0;
    }
}


void makeDB() {
    posix_memalign((void**) &Table, PAGE_SIZE, (tuple_num) * sizeof(Tuple));

    /*for (int i = 0; i < tuple_num; i++) {
        Table[i].min_cstamp_ = 0;
        posix_memalign((void**) &Table[i].latest_, CACHE_LINE_SIZE,
                       sizeof(Version));
        Version* verTmp = Table[i].latest_;
        verTmp->cstamp_ = 0;
        verTmp->pstamp_ = 0;
        verTmp->sstamp_ = UINT32_MAX & ~(1);
        // cstamp, sstamp の最下位ビットは TID フラグ
        // 1の時はTID, 0の時はstamp
        verTmp->prev_ = nullptr;
        verTmp->status_ = VersionStatus::committed;
        verTmp->readers_ = 0;
        }*/


    //size_t maxthread = decideParallelBuildNumber(tuple_num);
    size_t maxthread = 1;
    std::vector<std::thread> thv;
    for (size_t i = 0; i < maxthread; ++i)
        thv.emplace_back(partTableInit, i, i * (tuple_num / maxthread),
                         (i + 1) * (tuple_num / maxthread) - 1);
    for (auto& th : thv) th.join();
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

    RETRY:
        if (quit == true) break;

        trans.tbegin();
        for (auto itr = trans.task_set_.begin(); itr != trans.task_set_.end();
             ++itr) {
            if ((*itr).ope_ == Ope::READ) {
                trans.ssn_tread((*itr).key_);
            } else if ((*itr).ope_ == Ope::WRITE) {
                trans.ssn_twrite((*itr).key_);
            }
            //early abort.
            if (trans.status_ == TransactionStatus::aborted) {
                trans.abort();
                goto RETRY;
            }
        }
        trans.ssn_commit();
        if (trans.status_ == TransactionStatus::committed) {
            myres.local_commit_counts_++;
            std::cout << "txid_" << trans.txid_ << endl;
            std::cout << "cstamp_" << trans.cstamp_ << endl;
        } else if (trans.status_ == TransactionStatus::aborted) {
            trans.abort();
            goto RETRY;
        }
    }
    return;
}


int main(int argc, char* argv[]) {
    chkArg();
    makeDB();

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


    for (unsigned int i = 0; i < thread_num; ++i) {
        ErmiaResult[0].addLocalAllResult(ErmiaResult[i]);
    }
    ErmiaResult[0].displayAllResult();

    //displayDB();

    return 0;
}