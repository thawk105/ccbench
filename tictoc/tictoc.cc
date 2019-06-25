
#include <ctype.h>
#include <pthread.h>
#include <string.h>
#include <sched.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>

#define GLOBAL_VALUE_DEFINE

#include "include/common.hpp"
#include "include/result.hpp"
#include "include/transaction.hpp"

#include "../include/cpu.hpp"
#include "../include/debug.hpp"
#include "../include/masstree_wrapper.hpp"
#include "../include/random.hpp"
#include "../include/result.hpp"
#include "../include/tsc.hpp"
#include "../include/zipf.hpp"

extern void chkArg(const int argc, char *argv[]);
extern bool chkClkSpan(const uint64_t start, const uint64_t stop, const uint64_t threshold);
extern void displayDB();
extern void displayPRO();
extern void makeDB();
extern void makeProcedure(std::vector<Procedure>& pro, Xoroshiro128Plus &rnd, size_t& tuple_num, size_t& max_ope, size_t& rratio);
extern void makeProcedure(std::vector<Procedure>& pro, Xoroshiro128Plus &rnd, FastZipf &zipf, size_t& tuple_num, size_t& max_ope, size_t& rratio);
extern void makeProcedureWithCheckWriteOnly(std::vector<Procedure>& pro, Xoroshiro128Plus &rnd, FastZipf &zipf, size_t& tuple_num, size_t& max_ope, size_t& rratio, bool& wonly);
extern void ReadyAndWaitForReadyOfAllThread(std::atomic<size_t> &running, const size_t thnm);
extern void waitForReadyOfAllThread(std::atomic<size_t> &running, const size_t thnm);
extern void sleepMs(size_t ms);

static void *
worker(void *arg)
{
  TicTocResult &res = *(TicTocResult *)(arg);
  Xoroshiro128Plus rnd;
  rnd.init();
  TxExecutor trans(res.thid, &res);
  FastZipf zipf(&rnd, ZIPF_SKEW, TUPLE_NUM);

#if MASSTREE_USE
  MasstreeWrapper<Tuple>::thread_init(int(res.thid));
#endif

  setThreadAffinity(res.thid);
  //printf("Thread #%d: on CPU %d\n", *myid, sched_getcpu());
  //printf("sysconf(_SC_NPROCESSORS_CONF) %d\n", sysconf(_SC_NPROCESSORS_CONF));
  ReadyAndWaitForReadyOfAllThread(Running, THREAD_NUM);
  
  for (;;) {
    uint64_t start, stop;
    if (YCSB)
      makeProcedureWithCheckWriteOnly(trans.proSet, rnd, zipf, TUPLE_NUM, MAX_OPE, RRATIO, trans.wonly);
    else
      makeProcedure(trans.proSet, rnd, TUPLE_NUM, MAX_OPE, RRATIO);
RETRY:
    trans.tbegin();

    if (res.Finish.load(std::memory_order_acquire))
      return nullptr;

    start = rdtscp();
    for (auto itr = trans.proSet.begin(); itr != trans.proSet.end(); ++itr) {
      if ((*itr).ope == Ope::READ) {
        trans.tread((*itr).key);
      } else if ((*itr).ope == Ope::WRITE) {
        if (RMW) {
          trans.tread((*itr).key);
          if (trans.status != TransactionStatus::aborted)
            trans.twrite((*itr).key);
        }
        else
          trans.twrite((*itr).key);
      } else {
        ERR;
      }

      if (trans.status == TransactionStatus::aborted) {
        trans.abort();
        ++res.local_abort_counts;
        stop = rdtscp();
        res.local_read_latency += stop -start;
        goto RETRY;
      }
    }
    stop = rdtscp();
    res.local_read_latency += stop -start;
    
    //Validation phase
    start = rdtscp();
    bool varesult = trans.validationPhase();
    stop = rdtscp();
    res.local_vali_latency += stop - start;
    if (varesult) {
      trans.writePhase();
      ++res.local_commit_counts;
    } else {
      trans.abort();
      ++res.local_abort_counts;
      goto RETRY;
    }
  }

  return nullptr;
}

int 
main(int argc, char *argv[]) try
{
  chkArg(argc, argv);
  makeDB();
  
  TicTocResult rsob[THREAD_NUM];
  TicTocResult &rsroot = rsob[0];
  pthread_t thread[THREAD_NUM];
  for (unsigned int i = 0; i < THREAD_NUM; ++i) {
    int ret;
    rsob[i].thid = i;
    ret = pthread_create(&thread[i], NULL, worker, (void *)(&rsob[i]));
    if (ret) ERR;
  }

  waitForReadyOfAllThread(Running, THREAD_NUM);
  for (size_t i = 0; i < EXTIME; ++i) {
    sleepMs(1000);
  }
  TicTocResult::Finish.store(true, std::memory_order_release);

  for (unsigned int i = 0; i < THREAD_NUM; ++i) {
    pthread_join(thread[i], nullptr);
    rsroot.add_local_all_tictoc_result(rsob[i]);
  }

  rsroot.extime = EXTIME;
  rsroot.display_all_tictoc_result();

  return 0;
} catch (bad_alloc) {
  ERR;
}

