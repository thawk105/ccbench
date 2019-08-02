
#include <ctype.h>
#include <pthread.h>
#include <sched.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>

#define GLOBAL_VALUE_DEFINE

#include "../include/cpu.hh"
#include "../include/debug.hh"
#include "../include/masstree_wrapper.hh"
#include "../include/random.hh"
#include "../include/result.hh"
#include "../include/tsc.hh"
#include "../include/zipf.hh"
#include "include/common.hh"
#include "include/transaction.hh"

extern void chkArg(const int argc, char *argv[]);
extern bool chkClkSpan(const uint64_t start, const uint64_t stop,
                       const uint64_t threshold);
extern void displayDB();
extern void displayPRO();
extern void makeDB();
extern void makeProcedure(std::vector<Procedure> &pro, Xoroshiro128Plus &rnd,
                          FastZipf &zipf, size_t tuple_num, size_t max_ope,
                          size_t thread_num, size_t rratio, bool rmw, bool ycsb,
                          bool partition, size_t thread_id);
extern void ReadyAndWaitForReadyOfAllThread(std::atomic<size_t> &running,
                                            const size_t thnm);
extern void waitForReadyOfAllThread(std::atomic<size_t> &running,
                                    const size_t thnm);
extern void sleepMs(size_t ms);

static void *worker(void *arg) {
  Result &res = *(Result *)(arg);
  Xoroshiro128Plus rnd;
  rnd.init();
  TxExecutor trans(res.thid_, &res);
  FastZipf zipf(&rnd, ZIPF_SKEW, TUPLE_NUM);

#if MASSTREE_USE
  MasstreeWrapper<Tuple>::thread_init(int(res.thid_));
#endif

  setThreadAffinity(res.thid_);
  // printf("Thread #%d: on CPU %d\n", *myid, sched_getcpu());
  // printf("sysconf(_SC_NPROCESSORS_CONF) %d\n",
  // sysconf(_SC_NPROCESSORS_CONF));
  ReadyAndWaitForReadyOfAllThread(Running, THREAD_NUM);

  for (;;) {
    makeProcedure(trans.pro_set_, rnd, zipf, TUPLE_NUM, MAX_OPE, THREAD_NUM,
                  RRATIO, RMW, YCSB, false, res.thid_);
  RETRY:
    if (res.Finish_.load(std::memory_order_acquire)) return nullptr;
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

      if (trans.status_ == TransactionStatus::aborted) {
        trans.abort();
        goto RETRY;
      }
    }

    // Validation phase
    bool varesult = trans.validationPhase();
    if (varesult) {
      trans.writePhase();
    } else {
      trans.abort();
      goto RETRY;
    }
  }

  return nullptr;
}

int main(int argc, char *argv[]) try {
  chkArg(argc, argv);
  makeDB();

  Result rsob[THREAD_NUM];
  pthread_t thread[THREAD_NUM];
  for (unsigned int i = 0; i < THREAD_NUM; ++i) {
    int ret;
    rsob[i] = Result(CLOCKS_PER_US, EXTIME, i, THREAD_NUM);
    ret = pthread_create(&thread[i], NULL, worker, (void *)(&rsob[i]));
    if (ret) ERR;
  }

  waitForReadyOfAllThread(Running, THREAD_NUM);
  for (size_t i = 0; i < EXTIME; ++i) {
    sleepMs(1000);
  }
  Result::Finish_.store(true, std::memory_order_release);

  Result &rsroot = rsob[0];
  for (unsigned int i = 0; i < THREAD_NUM; ++i) {
    pthread_join(thread[i], nullptr);
    rsroot.addLocalAllResult(rsob[i]);
  }
  rsroot.displayAllResult();

  return 0;
} catch (bad_alloc) {
  ERR;
}
