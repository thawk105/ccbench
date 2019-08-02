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
#include <random>
#include <thread>

#define GLOBAL_VALUE_DEFINE
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

extern void chkArg(const int argc, char *argv[]);
extern bool chkClkSpan(const uint64_t start, const uint64_t stop,
                       const uint64_t threshold);
extern bool chkSpan(struct timeval &start, struct timeval &stop,
                    long threshold);
extern void makeDB(uint64_t *initial_wts);
extern void makeProcedure(std::vector<Procedure> &pro, Xoroshiro128Plus &rnd,
                          FastZipf &zipf, size_t tuple_num, size_t max_ope,
                          size_t thread_num, size_t rratio, bool rmw, bool ycsb,
                          bool partition, size_t thread_id);
extern void ReadyAndWaitForReadyOfAllThread(std::atomic<size_t> &running,
                                            size_t thnm);
extern void waitForReadyOfAllThread(std::atomic<size_t> &running, size_t thnm);
extern void sleepMs(size_t ms);

static void *manager_worker(void *arg) {
  TimeStamp tmp;

  uint64_t initial_wts;
  makeDB(&initial_wts);
  MinWts.store(initial_wts + 2, memory_order_release);
  Result &res = *(Result *)(arg);

#ifdef Linux
  setThreadAffinity(res.thid_);
  // printf("Thread #%d: on CPU %d\n", *myid, sched_getcpu());
#endif  // Linux

  ReadyAndWaitForReadyOfAllThread(Running, THREAD_NUM);
  while (FirstAllocateTimestamp.load(memory_order_acquire) != THREAD_NUM - 1) {
  }

  // leader work
  for (;;) {
    if (res.Finish_.load(std::memory_order_acquire)) return nullptr;

    bool gc_update = true;
    for (unsigned int i = 1; i < THREAD_NUM; ++i) {
      // check all thread's flag raising
      if (__atomic_load_n(&(GCFlag[i].obj_), __ATOMIC_ACQUIRE) == 0) {
        usleep(1);
        gc_update = false;
        break;
      }
    }
    if (gc_update) {
      uint64_t minw =
          __atomic_load_n(&(ThreadWtsArray[1].obj_), __ATOMIC_ACQUIRE);
      uint64_t minr;
      if (GROUP_COMMIT == 0) {
        minr = __atomic_load_n(&(ThreadRtsArray[1].obj_), __ATOMIC_ACQUIRE);
      } else {
        minr = __atomic_load_n(&(ThreadRtsArrayForGroup[1].obj_),
                               __ATOMIC_ACQUIRE);
      }

      for (unsigned int i = 1; i < THREAD_NUM; ++i) {
        uint64_t tmp =
            __atomic_load_n(&(ThreadWtsArray[i].obj_), __ATOMIC_ACQUIRE);
        if (minw > tmp) minw = tmp;
        if (GROUP_COMMIT == 0) {
          tmp = __atomic_load_n(&(ThreadRtsArray[i].obj_), __ATOMIC_ACQUIRE);
          if (minr > tmp) minr = tmp;
        } else {
          tmp = __atomic_load_n(&(ThreadRtsArrayForGroup[i].obj_),
                                __ATOMIC_ACQUIRE);
          if (minr > tmp) minr = tmp;
        }
      }

      MinWts.store(minw, memory_order_release);
      MinRts.store(minr, memory_order_release);

      // downgrade gc flag
      for (unsigned int i = 1; i < THREAD_NUM; ++i) {
        __atomic_store_n(&(GCFlag[i].obj_), 0, __ATOMIC_RELEASE);
        __atomic_store_n(&(GCExecuteFlag[i].obj_), 1, __ATOMIC_RELEASE);
      }
    }
  }

  return nullptr;
}

static void *worker(void *arg) {
  Result &res = *(Result *)(arg);
  Xoroshiro128Plus rnd;
  rnd.init();
  TxExecutor trans(res.thid_, (Result *)arg);
  FastZipf zipf(&rnd, ZIPF_SKEW, TUPLE_NUM);

#ifdef Linux
  setThreadAffinity(res.thid_);
  // printf("Thread #%d: on CPU %d\n", *myid, sched_getcpu());
  // printf("sysconf(_SC_NPROCESSORS_CONF) %d\n",
  // sysconf(_SC_NPROCESSORS_CONF));
#endif  // Linux

#ifdef Darwin
  int nowcpu;
  GETCPU(nowcpu);
  // printf("Thread %d on CPU %d\n", *myid, nowcpu);
#endif  // Darwin

  ReadyAndWaitForReadyOfAllThread(Running, THREAD_NUM);

  // printf("%s\n", trans.writeVal);
  // start work(transaction)
  for (;;) {
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
                  RRATIO, RMW, YCSB, true, res.thid_);
    sort(trans.pro_set_.begin(), trans.pro_set_.end());
#else
#if PARTITION_TABLE
    makeProcedure(trans.pro_set_, rnd, zipf, TUPLE_NUM, MAX_OPE, THREAD_NUM,
                  RRATIO, RMW, YCSB, true, res.thid_);
#else
    makeProcedure(trans.pro_set_, rnd, zipf, TUPLE_NUM, MAX_OPE, THREAD_NUM,
                  RRATIO, RMW, YCSB, false, res.thid_);
#endif
#endif

  RETRY:
    if (res.Finish_.load(std::memory_order_acquire)) return nullptr;

    trans.tbegin();
    // Read phase
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
      ++trans.continuing_commit_;
      ++res.local_commit_counts_;
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

  return nullptr;
}

int main(int argc, char *argv[]) try {
  chkArg(argc, argv);

  Result rsob[THREAD_NUM];
  Result &rsroot = rsob[0];
  pthread_t thread[THREAD_NUM];
  for (unsigned int i = 0; i < THREAD_NUM; ++i) {
    int ret;
    rsob[i] = Result(CLOCKS_PER_US, EXTIME, i, THREAD_NUM);
    if (i == 0)
      ret =
          pthread_create(&thread[i], NULL, manager_worker, (void *)(&rsob[i]));
    else
      ret = pthread_create(&thread[i], NULL, worker, (void *)(&rsob[i]));
    if (ret) ERR;
  }

  waitForReadyOfAllThread(Running, THREAD_NUM);
  for (size_t i = 0; i < EXTIME; ++i) {
    sleepMs(1000);
  }
  Result::Finish_.store(true, std::memory_order_release);

  for (unsigned int i = 0; i < THREAD_NUM; ++i) {
    pthread_join(thread[i], nullptr);
    rsroot.addLocalAllResult(rsob[i]);
  }
  rsroot.displayAllResult();

  return 0;
} catch (bad_alloc) {
  ERR;
}
