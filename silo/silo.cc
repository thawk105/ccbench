#include <ctype.h>
#include <pthread.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <algorithm>
#include <cctype>

#define GLOBAL_VALUE_DEFINE
#include "include/atomic_tool.hh"
#include "include/common.hh"
#include "include/transaction.hh"

#include "../include/atomic_wrapper.hh"
#include "../include/backoff.hh"
#include "../include/cpu.hh"
#include "../include/debug.hh"
#include "../include/fileio.hh"
#include "../include/masstree_wrapper.hh"
#include "../include/random.hh"
#include "../include/result.hh"
#include "../include/tsc.hh"
#include "../include/util.hh"
#include "../include/zipf.hh"

using namespace std;

extern void chkArg(const int argc, char* argv[]);
extern bool chkClkSpan(const uint64_t start, const uint64_t stop,
                       const uint64_t threshold);
extern bool chkEpochLoaded();
extern void displayParameter();
extern void isReady(const std::vector<char>& readys);
extern void leaderWork(uint64_t& epoch_timer_start, uint64_t& epoch_timer_stop);
extern void displayDB();
extern void displayPRO();
extern void genLogFile(std::string& logpath, const int thid);
extern void makeDB();
extern void waitForReady(const std::vector<char>& readys);
extern void sleepMs(size_t ms);

void worker(size_t thid, char& ready, const bool& start, const bool& quit,
            std::vector<Result>& res) {
  Result& myres = std::ref(res[thid]);
  Xoroshiro128Plus rnd;
  rnd.init();
  TxnExecutor trans(thid, (Result*)&myres);
  FastZipf zipf(&rnd, ZIPF_SKEW, TUPLE_NUM);
  uint64_t epoch_timer_start, epoch_timer_stop;
  Backoff backoff(CLOCKS_PER_US);

  // std::string logpath;
  // genLogFile(logpath, res.thid_);
  // trans.logfile.open(logpath, O_TRUNC | O_WRONLY, 0644);
  // trans.logfile.ftruncate(10^9);

#if MASSTREE_USE
  MasstreeWrapper<Tuple>::thread_init(int(thid));
#endif

#ifdef Linux
  setThreadAffinity(thid);
  // printf("Thread #%d: on CPU %d\n", res.thid_, sched_getcpu());
  // printf("sysconf(_SC_NPROCESSORS_CONF) %d\n",
  // sysconf(_SC_NPROCESSORS_CONF));
#endif

  storeRelease(ready, 1);
  while (!loadAcquire(start)) _mm_pause();
  if (thid == 0) epoch_timer_start = rdtscp();
  while (!loadAcquire(quit)) {
#if PARTITION_TABLE
    makeProcedure(trans.pro_set_, rnd, zipf, TUPLE_NUM, MAX_OPE, THREAD_NUM,
                  RRATIO, RMW, YCSB, true, thid, res);
#else
    makeProcedure(trans.pro_set_, rnd, zipf, TUPLE_NUM, MAX_OPE, THREAD_NUM,
                  RRATIO, RMW, YCSB, false, thid, myres);
#endif

#if PROCEDURE_SORT
    sort(trans.pro_set_.begin(), trans.pro_set_.end());
#endif

  RETRY:
    if (thid == 0) {
      leaderWork(epoch_timer_start, epoch_timer_stop);
      leaderBackoffWork(backoff, res);
      // printf("Thread #%d: on CPU %d\n", thid, sched_getcpu());
    }

    if (loadAcquire(quit)) break;

    trans.begin();
    for (auto itr = trans.pro_set_.begin(); itr != trans.pro_set_.end();
         ++itr) {
      if ((*itr).ope_ == Ope::READ) {
        trans.read((*itr).key_);
      } else if ((*itr).ope_ == Ope::WRITE) {
        trans.write((*itr).key_);
      } else if ((*itr).ope_ == Ope::READ_MODIFY_WRITE) {
        trans.read((*itr).key_);
        trans.write((*itr).key_);
      } else {
        ERR;
      }
    }

    if (trans.validationPhase()) {
      trans.writePhase();
      ++myres.local_commit_counts_;
    } else {
      trans.abort();
      ++myres.local_abort_counts_;
      goto RETRY;
    }
  }

  return;
}

int main(int argc, char* argv[]) try {
  chkArg(argc, argv);
  // displayParameter();
  makeDB();

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

  return 0;
} catch (bad_alloc) {
  ERR;
}
