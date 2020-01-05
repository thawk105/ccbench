#include <ctype.h>
#include <pthread.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <functional>
#include <random>
#include <thread>

#define GLOBAL_VALUE_DEFINE
#include "../include/atomic_wrapper.hh"
#include "../include/backoff.hh"
#include "../include/compiler.hh"
#include "../include/cpu.hh"
#include "../include/debug.hh"
#include "../include/int64byte.hh"
#include "../include/procedure.hh"
#include "../include/random.hh"
#include "../include/result.hh"
#include "../include/zipf.hh"
#include "include/common.hh"
#include "include/transaction.hh"

using namespace std;

extern void chkArg(const int argc, char* argv[]);
extern bool chkClkSpan(const uint64_t start, const uint64_t stop,
                       const uint64_t threshold);
extern bool chkSpan(struct timeval& start, struct timeval& stop,
                    long threshold);
extern void deleteDB();
extern void isReady(const std::vector<char>& readys);
extern void makeDB(uint64_t* initial_wts);
extern void leaderWork([[maybe_unused]] Backoff& backoff,
                       [[maybe_unused]] std::vector<Result>& res);
extern void waitForReady(const std::vector<char>& readys);
extern void sleepMs(size_t ms);

void worker(size_t thid, char& ready, const bool& start, const bool& quit,
            std::vector<Result>& res) {
  Xoroshiro128Plus rnd;
  rnd.init();
  TxExecutor trans(thid, (Result*)&res[thid]);
  Result& myres = std::ref(res[thid]);
  FastZipf zipf(&rnd, ZIPF_SKEW, TUPLE_NUM);
  Backoff backoff(CLOCKS_PER_US);

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
    makeProcedure(trans.pro_set_, rnd, zipf, TUPLE_NUM, MAX_OPE, THREAD_NUM,
                  RRATIO, RMW, YCSB, true, thid, myres);
    sort(trans.pro_set_.begin(), trans.pro_set_.end());
#else
#if PARTITION_TABLE
    makeProcedure(trans.pro_set_, rnd, zipf, TUPLE_NUM, MAX_OPE, THREAD_NUM,
                  RRATIO, RMW, YCSB, true, thid, myres);
#else
    makeProcedure(trans.pro_set_, rnd, zipf, TUPLE_NUM, MAX_OPE, THREAD_NUM,
                  RRATIO, RMW, YCSB, false, thid, myres);
#endif
#endif

  RETRY:
    if (thid == 0) leaderWork(std::ref(backoff), std::ref(res));
    if (loadAcquire(quit)) break;

    trans.tbegin();
    for (auto itr = trans.pro_set_.begin(); itr != trans.pro_set_.end();
         ++itr) {
      if ((*itr).ope_ == Ope::READ) {
        trans.tread((*itr).key_);
      } else if ((*itr).ope_ == Ope::WRITE) {
        trans.twrite((*itr).key_);
      } else if ((*itr).ope_ == Ope::READ_MODIFY_WRITE) {
        trans.tread((*itr).key_);
        trans.twrite((*itr).key_);
      } else {
        ERR;
      }

      if (trans.status_ == TransactionStatus::abort) {
        trans.earlyAbort();
        goto RETRY;
      }
    }

    // read only tx doesn't collect read set and doesn't validate.
    // write phase execute logging and commit pending versions, but r-only tx
    // can skip it.
    if ((*trans.pro_set_.begin()).ronly_) {
      ++res[thid].local_commit_counts_;
    } else {
      // Validation phase
      if (!trans.validation()) {
        trans.abort();
        goto RETRY;
      }

      // Write phase
      trans.writePhase();

      // Maintenance
      // Schedule garbage collection
      // Declare quiescent state
      // Collect garbage created by prior transactions
#if SINGLE_EXEC
#else
      trans.mainte();
#endif
    }
  }

  return;
}

int main(int argc, char* argv[]) try {
  chkArg(argc, argv);
  uint64_t initial_wts;
  makeDB(&initial_wts);
  MinWts.store(initial_wts + 2, memory_order_release);

  alignas(CACHE_LINE_SIZE) bool start = false;
  alignas(CACHE_LINE_SIZE) bool quit = false;
  alignas(CACHE_LINE_SIZE) std::vector<Result> res(THREAD_NUM);
  std::vector<char> readys(THREAD_NUM);
  std::vector<std::thread> thv;
  for (size_t i = 0; i < THREAD_NUM; ++i)
    thv.emplace_back(worker, i, std::ref(readys[i]), std::ref(start),
                     std::ref(quit), std::ref(res));
  waitForReady(readys);
  storeRelease(start, true);
  for (size_t i = 0; i < EXTIME; ++i) {
    sleepMs(1000);
  }
  storeRelease(quit, true);
  for (auto& th : thv) th.join();

  for (unsigned int i = 0; i < THREAD_NUM; ++i) {
    res[0].addLocalAllResult(res[i]);
  }
  res[0].displayAllResult(CLOCKS_PER_US, EXTIME, THREAD_NUM);
  deleteDB();

  return 0;
} catch (bad_alloc) {
  ERR;
}
