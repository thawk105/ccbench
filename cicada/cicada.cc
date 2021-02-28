#include <pthread.h>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <cstdint>
#include <functional>
#include <thread>

#define GLOBAL_VALUE_DEFINE

#include "../include/atomic_wrapper.hh"
#include "../include/backoff.hh"
#include "include/common.hh"
#include "include/result.hh"
#include "include/transaction.hh"
#include "include/util.hh"

using namespace std;

void worker(size_t thid, char &ready, const bool &start, const bool &quit) {
    Xoroshiro128Plus rnd;
    rnd.init();
    TxExecutor trans(thid, (Result*) &CicadaResult[thid]);
    Result &myres = std::ref(CicadaResult[thid]);
    FastZipf zipf(&rnd, FLAGS_zipf_skew, FLAGS_tuple_num);
    Backoff backoff(FLAGS_clocks_per_us);

#ifdef Linux
    setThreadAffinity(thid);
    // printf("Thread #%d: on CPU %d\n", *myid, sched_getcpu());
    // printf("sysconf(_SC_NPROCESSORS_CONF) %d\n",
    // sysconf(_SC_NPROCESSORS_CONF));
#endif  // Linux

#ifdef Darwin
    int nowcpu;
    GETCPU(nowcpu);
    // printf("Thread %d on CPU %d\n", *myid, nowcpu);
#endif  // Darwin

    storeRelease(ready, 1);
    while (!loadAcquire(start)) _mm_pause();
    while (!loadAcquire(quit)) {
        /* シングル実行で絶対に競合を起こさないワークロードにおいて，
         * 自トランザクションで read した後に write するのは複雑になる．
         * write した後に read であれば，write set から read
         * するので挙動がシンプルになる．
         * スレッドごとにアクセスブロックを作る形でパーティションを作って
         * スレッド間の競合を無くした後に sort して同一キーに対しては
         * write - read とする．
         * */
#if SINGLE_EXEC
        makeProcedure(trans.pro_set_, rnd, zipf, FLAGS_tuple_num, FLAGS_max_ope, FLAGS_thread_num,
                      FLAGS_rratio, FLAGS_rmw, FLAGS_ycsb, true, thid, myres);
        sort(trans.pro_set_.begin(), trans.pro_set_.end());
#else
#if PARTITION_TABLE
        makeProcedure(trans.pro_set_, rnd, zipf, FLAGS_tuple_num, FLAGS_max_ope, FLAGS_thread_num,
                      FLAGS_rratio, FLAGS_rmw, FLAGS_ycsb, true, thid, myres);
#else
        makeProcedure(trans.pro_set_, rnd, zipf, FLAGS_tuple_num, FLAGS_max_ope, FLAGS_thread_num,
                      FLAGS_rratio, FLAGS_rmw, FLAGS_ycsb, false, thid, myres);
#endif
#endif

RETRY:
        if (thid == 0) {
            leaderWork(std::ref(backoff));
#if BACK_OFF
            leaderBackoffWork(backoff, CicadaResult);
#endif
        }
        if (loadAcquire(quit)) break;

        trans.tbegin();
        for (auto &&itr : trans.pro_set_) {
            if ((itr).ope_ == Ope::READ) {
                trans.tread((itr).key_);
            } else if ((itr).ope_ == Ope::WRITE) {
                trans.twrite((itr).key_);
            } else if ((itr).ope_ == Ope::READ_MODIFY_WRITE) {
                trans.tread((itr).key_);
                trans.twrite((itr).key_);
            } else {
                ERR;
            }

            if (trans.status_ == TransactionStatus::abort) {
                trans.earlyAbort();
#if SINGLE_EXEC
#else
                trans.mainte();
#endif
                goto RETRY;
            }
        }

        /**
         * Tanabe Optimization for analysis
         */
#if WORKER1_INSERT_DELAY_RPHASE
        if (unlikely(thid == 1) && WORKER1_INSERT_DELAY_RPHASE_US != 0) {
          clock_delay(WORKER1_INSERT_DELAY_RPHASE_US * FLAGS_clocks_per_us);
        }
#endif

        /**
         * Excerpt from original paper 3.1 Multi-Clocks Timestamp Allocation
         * A read-only transaction uses (thread.rts) instead,
         * and does not track or validate the read set;
         */
        if ((*trans.pro_set_.begin()).ronly_) {
            /**
             * local_commit_counts is used at ../include/backoff.hh to calcurate about
             * backoff.
             */
            storeRelease(myres.local_commit_counts_,
                         loadAcquire(myres.local_commit_counts_) + 1);
        } else {
            /**
             * Validation phase
             */
            if (!trans.validation()) {
                trans.abort();
#if SINGLE_EXEC
#else
                /**
                 * Maintenance phase
                 */
                trans.mainte();
#endif
                goto RETRY;
            }

            /**
             * Write phase
             */
            trans.writePhase();
            /**
             * local_commit_counts is used at ../include/backoff.hh to calcurate about
             * backoff.
             */
            storeRelease(myres.local_commit_counts_,
                         loadAcquire(myres.local_commit_counts_) + 1);

            /**
             * Maintenance phase
             */
#if SINGLE_EXEC
#else
            trans.mainte();
#endif
        }
    }
}

int main(int argc, char* argv[]) try {
    gflags::SetUsageMessage("Cicada benchmark.");
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    chkArg();
    uint64_t initial_wts;
    makeDB(&initial_wts);
    MinWts.store(initial_wts + 2, memory_order_release);

    alignas(CACHE_LINE_SIZE) bool start = false;
    alignas(CACHE_LINE_SIZE) bool quit = false;
    initResult();
    std::vector<char> readys(FLAGS_thread_num);
    std::vector<std::thread> thv;
    for (size_t i = 0; i < FLAGS_thread_num; ++i)
        thv.emplace_back(worker, i, std::ref(readys[i]), std::ref(start),
                         std::ref(quit));
    waitForReady(readys);
    storeRelease(start, true);
    for (size_t i = 0; i < FLAGS_extime; ++i) {
        sleepMs(1000);
    }
    storeRelease(quit, true);
    for (auto &th : thv) th.join();

    for (unsigned int i = 0; i < FLAGS_thread_num; ++i) {
        CicadaResult[0].addLocalAllResult(CicadaResult[i]);
    }
    ShowOptParameters();
    CicadaResult[0].displayAllResult(FLAGS_clocks_per_us, FLAGS_extime, FLAGS_thread_num);
    deleteDB();

    return 0;
} catch (std::bad_alloc&) {
    ERR;
}
