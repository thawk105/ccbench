
#include <atomic>
#include <bitset>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>

// ccbench/cicada/include
#include "include/common.hh"
#include "include/time_stamp.hh"
#include "include/transaction.hh"
#include "include/util.hh"

// ccbench/include/
#include "backoff.hh"
#include "config.hh"
#include "logger.h"
#include "masstree_wrapper.hh"

using std::cout, std::endl;

void chkArg() {
    displayParameter();

    if (FLAGS_rratio > 100) {
        SPDLOG_INFO("rratio [%%] must be 0 ~ 100)");
        ERR;
    }

    if (FLAGS_zipf_skew >= 1) {
        SPDLOG_INFO("FLAGS_zipf_skew must be 0 ~ 0.999...");
        ERR;
    }

    if (FLAGS_clocks_per_us < 100) {
        SPDLOG_INFO("CPU_MHZ is less than 100. are you really?\n");
        exit(0);
    }

    if (posix_memalign((void**) &ThreadRtsArrayForGroup, CACHE_LINE_SIZE,
                       FLAGS_thread_num * sizeof(uint64_t_64byte)) != 0)
        ERR;
    if (posix_memalign((void**) &ThreadWtsArray, CACHE_LINE_SIZE,
                       FLAGS_thread_num * sizeof(uint64_t_64byte)) != 0)
        ERR;
    if (posix_memalign((void**) &ThreadRtsArray, CACHE_LINE_SIZE,
                       FLAGS_thread_num * sizeof(uint64_t_64byte)) != 0)
        ERR;
    if (posix_memalign((void**) &GROUP_COMMIT_INDEX, CACHE_LINE_SIZE,
                       FLAGS_thread_num * sizeof(uint64_t_64byte)) != 0)
        ERR;
    if (posix_memalign((void**) &GROUP_COMMIT_COUNTER, CACHE_LINE_SIZE,
                       FLAGS_thread_num * sizeof(uint64_t_64byte)) != 0)
        ERR;
    if (posix_memalign((void**) &GCFlag, CACHE_LINE_SIZE,
                       FLAGS_thread_num * sizeof(uint64_t_64byte)) != 0)
        ERR;
    if (posix_memalign((void**) &GCExecuteFlag, CACHE_LINE_SIZE,
                       FLAGS_thread_num * sizeof(uint64_t_64byte)) != 0)
        ERR;

    SLogSet = new Version* [(FLAGS_max_ope) * (FLAGS_group_commit)];
    PLogSet = new Version** [FLAGS_thread_num];

    for (unsigned int i = 0; i < FLAGS_thread_num; ++i) {
        PLogSet[i] = new Version* [(FLAGS_max_ope) * (FLAGS_group_commit)];
    }

    // init
    for (unsigned int i = 0; i < FLAGS_thread_num; ++i) {
        GCFlag[i].obj_ = 0;
        GCExecuteFlag[i].obj_ = 0;
        GROUP_COMMIT_INDEX[i].obj_ = 0;
        GROUP_COMMIT_COUNTER[i].obj_ = 0;
        ThreadRtsArray[i].obj_ = 0;
        ThreadWtsArray[i].obj_ = 0;
        ThreadRtsArrayForGroup[i].obj_ = 0;
    }
}

[[maybe_unused]] void displayDB() {
    Tuple* tuple;
    Version* version;

    for (unsigned int i = 0; i < FLAGS_tuple_num; ++i) {
        tuple = &Table[i % FLAGS_tuple_num];
        cout << "------------------------------" << endl;  //-は30個
        cout << "key: " << i << endl;

        version = tuple->latest_;
        while (version != nullptr) {
            cout << "val: " << version->val_ << endl;

            switch (version->status_) {
                case VersionStatus::invalid:
                    cout << "status:  invalid";
                    break;
                case VersionStatus::pending:
                    cout << "status:  pending";
                    break;
                case VersionStatus::aborted:
                    cout << "status:  aborted";
                    break;
                case VersionStatus::committed:
                    cout << "status:  committed";
                    break;
                case VersionStatus::deleted:
                    cout << "status:  deleted";
                    break;
                default:
                    cout << "status error";
                    break;
            }
            cout << endl;

            cout << "wts: " << version->wts_ << endl;
            cout << "bit: " << static_cast<bitset<64> >(version->wts_) << endl;
            cout << "rts: " << version->rts_ << endl;
            cout << "bit: " << static_cast<bitset<64> >(version->rts_) << endl;
            cout << endl;

            version = version->next_;
        }
        cout << endl;
    }
}

[[maybe_unused]] void displayMinRts() { cout << "MinRts:  " << MinRts << endl << endl; }

[[maybe_unused]] void displayMinWts() { cout << "MinWts:  " << MinWts << endl << endl; }

void displayParameter() {
    cout << "#FLAGS_clocks_per_us:\t\t\t" << FLAGS_clocks_per_us << endl;
    cout << "#FLAGS_extime:\t\t\t\t" << FLAGS_extime << endl;
    cout << "#FLAGS_gc_inter_us:\t\t\t" << FLAGS_gc_inter_us << endl;
    cout << "#FLAGS_group_commit:\t\t\t" << FLAGS_group_commit << endl;
    cout << "#FLAGS_group_commit_timeout_us:\t\t" << FLAGS_group_commit_timeout_us << endl;
    cout << "#FLAGS_io_time_ns:\t\t\t" << FLAGS_io_time_ns << endl;
    cout << "#FLAGS_max_ope:\t\t\t\t" << FLAGS_max_ope << endl;
    cout << "#FLAGS_pre_reserve_version:\t\t" << FLAGS_pre_reserve_version << endl;
    cout << "#FLAGS_p_wal:\t\t\t\t" << FLAGS_p_wal << endl;
    cout << "#FLAGS_rmw:\t\t\t\t" << FLAGS_rmw << endl;
    cout << "#FLAGS_rratio:\t\t\t\t" << FLAGS_rratio << endl;
    cout << "#FLAGS_s_wal:\t\t\t\t" << FLAGS_s_wal << endl;
    cout << "#FLAGS_thread_num:\t\t\t" << FLAGS_thread_num << endl;
    cout << "#FLAGS_ycsb:\t\t\t\t" << FLAGS_ycsb << endl;
    cout << "#FLAGS_worker1_insert_delay_rphase_us:\t" << FLAGS_worker1_insert_delay_rphase_us << endl;
    cout << "#FLAGS_zipf_skew:\t\t\t" << FLAGS_zipf_skew << endl;
}

[[maybe_unused]] void displaySLogSet() {
    if (!FLAGS_group_commit) {
    } else {
        if (FLAGS_s_wal) {
            SwalLock.w_lock();
            for (unsigned int i = 0; i < GROUP_COMMIT_INDEX[0].obj_; ++i) {
                // printf("SLogSet[%d]->key, val = (%d, %d)\n", i, SLogSet[i]->key,
                // SLogSet[i]->val);
            }
            SwalLock.w_unlock();

            // if (i == 0) printf("SLogSet is empty\n");
        }
    }
}

[[maybe_unused]] void displayThreadWtsArray() {
    cout << "ThreadWtsArray:" << endl;
    for (unsigned int i = 0; i < FLAGS_thread_num; i++) {
        cout << "thid " << i << ": " << ThreadWtsArray[i].obj_ << endl;
    }
    cout << endl << endl;
}

[[maybe_unused]] void displayThreadRtsArray() {
    cout << "ThreadRtsArray:" << endl;
    for (unsigned int i = 0; i < FLAGS_thread_num; i++) {
        cout << "thid " << i << ": " << ThreadRtsArray[i].obj_ << endl;
    }
    cout << endl << endl;
}

void partTableInit([[maybe_unused]] size_t thid, uint64_t initts,
                   uint64_t start, uint64_t end) {
#if MASSTREE_USE
    MasstreeWrapper<Tuple>::thread_init(thid);
#endif

    for (uint64_t i = start; i <= end; ++i) {
        Tuple* tuple;
        tuple = TxExecutor::get_tuple(Table, i);
        tuple->min_wts_ = initts;
        tuple->gc_lock_.store(0, std::memory_order_release);
        tuple->continuing_commit_.store(0, std::memory_order_release);

#if INLINE_VERSION_OPT
        tuple->latest_ = &tuple->inline_ver_;
        tuple->inline_ver_.set(0, initts, nullptr, VersionStatus::committed);
        tuple->inline_ver_.val_[0] = '\0';
#else
        tuple->latest_.store(new Version(), std::memory_order_release);
        (tuple->latest_.load(std::memory_order_acquire))
                ->set(0, initts, nullptr, VersionStatus::committed);
        (tuple->latest_.load(std::memory_order_acquire))->val_[0] = '\0';
#endif

#if MASSTREE_USE
        MT.insert_value(i, tuple);
#endif
    }
}

void partTableDelete([[maybe_unused]] size_t thid, uint64_t start,
                     uint64_t end) {
    for (uint64_t i = start; i <= end; ++i) {
        Tuple* tuple;
        tuple = TxExecutor::get_tuple(Table, i);
        Version* ver = tuple->latest_;
        while (ver != nullptr) {
#if INLINE_VERSION_OPT
            if (ver == &tuple->inline_ver_) {
                ver = ver->next_.load(memory_order_acquire);
                continue;
            }
#endif
            Version* del = ver;
            ver = ver->next_.load(memory_order_acquire);
            delete del;
        }
    }
}

void deleteDB() {
    size_t maxthread = decideParallelBuildNumber(FLAGS_tuple_num);
    std::vector<std::thread> thv;
    for (size_t i = 0; i < maxthread; ++i)
        thv.emplace_back(partTableDelete, i, i * (FLAGS_tuple_num / maxthread),
                         (i + 1) * (FLAGS_tuple_num / maxthread) - 1);
    for (auto &th : thv) th.join();

    delete Table;
    delete ThreadRtsArrayForGroup;
    delete ThreadWtsArray;
    delete ThreadRtsArray;
    delete GROUP_COMMIT_INDEX;
    delete GROUP_COMMIT_COUNTER;
    delete GCFlag;
    delete GCExecuteFlag;
    delete SLogSet;
    for (uint i = 0; i < FLAGS_thread_num; ++i) delete PLogSet[i];
    delete PLogSet;
}

void makeDB(uint64_t* initial_wts) {
    if (posix_memalign((void**) &Table, PAGE_SIZE, FLAGS_tuple_num * sizeof(Tuple)) !=
        0)
        ERR;
#if dbs11
    if (madvise((void *)Table, (FLAGS_tuple_num) * sizeof(Tuple), MADV_HUGEPAGE) != 0)
      ERR;
#endif

    TimeStamp tstmp;
    tstmp.generateTimeStampFirst(0);
    *initial_wts = tstmp.ts_;

    size_t maxthread = decideParallelBuildNumber(FLAGS_tuple_num);
    std::vector<std::thread> thv;
    for (size_t i = 0; i < maxthread; ++i)
        thv.emplace_back(partTableInit, i, tstmp.ts_, i * (FLAGS_tuple_num / maxthread),
                         (i + 1) * (FLAGS_tuple_num / maxthread) - 1);
    for (auto &th : thv) th.join();
}

void leaderWork([[maybe_unused]] Backoff &backoff) {
    bool gc_update{true};
    for (unsigned int i = 0; i < FLAGS_thread_num; ++i) {
        // check all thread's flag raising
        if (__atomic_load_n(&(GCFlag[i].obj_), __ATOMIC_ACQUIRE) == 0) {
            gc_update = false;
            break;
        }
    }
    if (gc_update) {
        // It initializes min w/r by worker whose id is 0.
        uint64_t minw = __atomic_load_n(&(ThreadWtsArray[0].obj_), __ATOMIC_ACQUIRE);
        uint64_t minr;
        if (FLAGS_group_commit == 0) {
            minr = __atomic_load_n(&(ThreadRtsArray[0].obj_), __ATOMIC_ACQUIRE);
        } else {
            minr = __atomic_load_n(&(ThreadRtsArrayForGroup[0].obj_), __ATOMIC_ACQUIRE);
        }

        // It already included value of worker whose id is 0, so starts from id 1.
        for (unsigned int i = 1; i < FLAGS_thread_num; ++i) {
            uint64_t tmp = __atomic_load_n(&(ThreadWtsArray[i].obj_), __ATOMIC_ACQUIRE);
            if (minw > tmp) minw = tmp;

            if (FLAGS_group_commit == 0) {
                tmp = __atomic_load_n(&(ThreadRtsArray[i].obj_), __ATOMIC_ACQUIRE);
            } else {
                tmp = __atomic_load_n(&(ThreadRtsArrayForGroup[i].obj_), __ATOMIC_ACQUIRE);
            }
            if (minr > tmp) minr = tmp;
        }

        MinWts.store(minw, memory_order_release);
        MinRts.store(minr, memory_order_release);

        // downgrade gc flag
        for (unsigned int i = 0; i < FLAGS_thread_num; ++i) {
            __atomic_store_n(&(GCFlag[i].obj_), 0, __ATOMIC_RELEASE);
            __atomic_store_n(&(GCExecuteFlag[i].obj_), 1, __ATOMIC_RELEASE);
        }
    }
}

void ShowOptParameters() {
    cout << "#ShowOptParameters()"
         << ": ADD_ANALYSIS " << ADD_ANALYSIS << ": BACK_OFF " << BACK_OFF
         << ": INLINE_VERSION_OPT " << INLINE_VERSION_OPT
         << ": INLINE_VERSION_PROMOTION " << INLINE_VERSION_PROMOTION
         << ": MASSTREE_USE " << MASSTREE_USE << ": PARTITION_TABLE "
         << PARTITION_TABLE << ": REUSE_VERSION " << REUSE_VERSION
         << ": SINGLE_EXEC " << SINGLE_EXEC << ": KEY_SIZE " << KEY_SIZE
         << ": VAL_SIZE " << VAL_SIZE << ": WRITE_LATEST_ONLY "
         << WRITE_LATEST_ONLY << ": WORKER1_INSERT_DELAY_RPHASE "
         << WORKER1_INSERT_DELAY_RPHASE << endl;
}
