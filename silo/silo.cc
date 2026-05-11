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

#include "boost/filesystem.hpp"

#define GLOBAL_VALUE_DEFINE

#include "include/atomic_tool.hh"
#include "include/common.hh"
#include "include/result.hh"
#include "include/transaction.hh"
#include "include/util.hh"

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
#include "../include/ycsb.hh"
#include "../include/zipf.hh"

using namespace std;

void worker(size_t thid, char &ready, const bool &start, const bool &quit) {
  Result &myres = std::ref(SiloResult[thid]);
  // Xoroshiro128Plus rnd;
  // rnd.init();
  TxExecutor trans(thid, (Result *) &myres, quit);
  Workload workload;
  // FastZipf zipf(&rnd, FLAGS_zipf_skew, FLAGS_tuple_num);
  // uint64_t epoch_timer_start, epoch_timer_stop;
#if BACK_OFF
  Backoff backoff(FLAGS_clocks_per_us);
#endif

#if WAL
  /*
  const boost::filesystem::path log_dir_path("/tmp/ccbench");
  if (boost::filesystem::exists(log_dir_path)) {
  } else {
    boost::system::error_code error;
    const bool result = boost::filesystem::create_directory(log_dir_path,
  error); if (!result || error) { ERR;
    }
  }
  std::string logpath("/tmp/ccbench");
  */
  std::string logpath;
  genLogFile(logpath, thid);
  trans.logfile_.open(logpath, O_CREAT | O_TRUNC | O_WRONLY, 0644);
  trans.logfile_.ftruncate(10 ^ 9);
#endif

#ifdef Linux
  setThreadAffinity(thid);
  // printf("Thread #%d: on CPU %d\n", res.thid_, sched_getcpu());
  // printf("sysconf(_SC_NPROCESSORS_CONF) %d\n",
  // sysconf(_SC_NPROCESSORS_CONF));
#endif

#if MASSTREE_USE
  MasstreeWrapper<Tuple>::thread_init(int(thid));
#endif

  // uint64_t tuples = FLAGS_tuple_num;
  // if (FLAGS_batch_simple_rr) {
  //   tuples = FLAGS_tuple_num - FLAGS_batch_tuples;
  // }

  storeRelease(ready, 1);
  while (!loadAcquire(start)) _mm_pause();
  if (thid == 0) trans.epoch_timer_start = rdtscp();
  while (!loadAcquire(quit)) {
    workload.run<TxExecutor,TransactionStatus>(trans);
// #if PARTITION_TABLE
//     makeProcedure(trans.pro_set_, rnd, zipf, FLAGS_tuple_num, FLAGS_max_ope,
//                   FLAGS_thread_num, FLAGS_rratio, FLAGS_rmw, FLAGS_ycsb, true,
//                   thid, myres);
// #else
//     auto r = rnd.next() % 100;
//     if ((FLAGS_thread_num && thid >= FLAGS_thread_num)
//       || (r < FLAGS_batch_ratio)) {
//       trans.is_batch_ = true;
//       makeBatchProcedure(trans.pro_set_, rnd, FLAGS_tuple_num, FLAGS_batch_tuples,
//                          FLAGS_batch_max_ope, FLAGS_batch_rratio, FLAGS_rmw,
//                          myres);
//     } else if (r >= FLAGS_batch_ratio
//                 && r < FLAGS_batch_ratio + FLAGS_ronly_ratio) {
//       trans.is_batch_ = false;
//       makeProcedure(trans.pro_set_, rnd, FLAGS_tuple_num, FLAGS_batch_tuples,
//                     FLAGS_max_ope, myres);
//     } else {
//       trans.is_batch_ = false;
//       makeProcedure(trans.pro_set_, rnd, zipf, tuples, FLAGS_max_ope,
//                     FLAGS_thread_num, FLAGS_rratio, FLAGS_rmw, FLAGS_ycsb,
//                     false, thid, myres);
//     }
// #endif

// #if PROCEDURE_SORT
//     sort(trans.pro_set_.begin(), trans.pro_set_.end());
// #endif

// RETRY:
//     if (thid == 0) {
//       leaderWork(epoch_timer_start, epoch_timer_stop);
// #if BACK_OFF
//       leaderBackoffWork(backoff, SiloResult);
// #endif
//       // printf("Thread #%d: on CPU %d\n", thid, sched_getcpu());
//     }

//     if (loadAcquire(quit)) break;

//     trans.begin();
//     for (auto itr = trans.pro_set_.begin(); itr != trans.pro_set_.end();
//          ++itr) {
//       if ((*itr).ope_ == Ope::READ) {
//         trans.read((*itr).key_);
// #ifdef INSERT_READ_DELAY_MS
//         sleepMs(INSERT_READ_DELAY_MS);
// #endif
//       } else if ((*itr).ope_ == Ope::WRITE) {
//         trans.write((*itr).key_);
//       } else if ((*itr).ope_ == Ope::READ_MODIFY_WRITE) {
//         trans.read((*itr).key_);
// #ifdef INSERT_READ_DELAY_MS
//         sleepMs(INSERT_READ_DELAY_MS);
// #endif
//         trans.write((*itr).key_);
//       } else {
//         ERR;
//       }
//     }

// #ifdef INSERT_BATCH_DELAY_MS
//     if (trans.is_batch_) {
//       sleepMs(INSERT_BATCH_DELAY_MS);
//     }
// #endif

//     if (trans.validationPhase()) {
//       trans.writePhase();
//       /**
//        * local_commit_counts is used at ../include/backoff.hh to calcurate about
//        * backoff.
//        */
//       if (trans.is_batch_) {
//         storeRelease(myres.local_batch_commit_counts_,
//                     loadAcquire(myres.local_batch_commit_counts_) + 1);
//       } else {
//         storeRelease(myres.local_commit_counts_,
//                     loadAcquire(myres.local_commit_counts_) + 1);
//       }
//     } else {
//       trans.abort();
//       if (trans.is_batch_) {
//         ++myres.local_batch_abort_counts_;
//       } else {
//         ++myres.local_abort_counts_;
//       }
//       goto RETRY;
//     }
  }

  return;
}

int main(int argc, char *argv[]) try {
  gflags::SetUsageMessage("Silo benchmark.");
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  chkArg();
  Workload::displayWorkloadParameter();
  Workload::makeDB<Tuple,void>(nullptr);

  alignas(CACHE_LINE_SIZE) bool start = false;
  alignas(CACHE_LINE_SIZE) bool quit = false;
  initResult();
  std::vector<char> readys(TotalThreadNum);
  std::vector<std::thread> thv;
  for (size_t i = 0; i < TotalThreadNum; ++i)
    thv.emplace_back(worker, i, std::ref(readys[i]), std::ref(start),
                     std::ref(quit));
  waitForReady(readys);
  uint64_t start_tsc = rdtscp();
  storeRelease(start, true);
  for (size_t i = 0; i < FLAGS_extime; ++i) {
    sleepMs(1000);
  }
  storeRelease(quit, true);
  for (auto &th : thv) th.join();
  uint64_t end_tsc = rdtscp();
  long double actual_extime = round(
    (end_tsc-start_tsc) /
    ((long double)FLAGS_clocks_per_us * powl(10.0, 6.0)));

  for (unsigned int i = 0; i < TotalThreadNum; ++i) {
    SiloResult[0].addLocalAllResult(SiloResult[i]);
  }
  ShowOptParameters();
  std::cout << "actual_extime:\t" << actual_extime << std::endl;
  SiloResult[0].displayAllResult(FLAGS_clocks_per_us, FLAGS_extime, TotalThreadNum);

  return 0;
} catch (bad_alloc) {
  ERR;
}
