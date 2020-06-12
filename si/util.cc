
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>  // syscall(SYS_gettid),
#include <sys/types.h>    // syscall(SYS_gettid),
#include <unistd.h>       // syscall(SYS_gettid),
#include <atomic>
#include <bitset>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>

#include "../include/config.hh"
#include "../include/debug.hh"
#include "../include/masstree_wrapper.hh"
#include "../include/procedure.hh"
#include "../include/random.hh"
#include "../include/result.hh"
#include "../include/tsc.hh"
#include "../include/zipf.hh"
#include "include/common.hh"
#include "include/util.hh"

void chkArg() {
  displayParameter();

  if (FLAGS_thread_num < 1) {
    cout << "1 thread is minimum."
            "so exit."
         << endl;
    ERR;
  }
  if (FLAGS_rratio > 100) {
    cout << "rratio [%%] must be 0 ~ 100" << endl;
    ERR;
  }

  if (FLAGS_zipf_skew >= 1) {
    cout << "FLAGS_zipf_skew must be 0 ~ 0.999..." << endl;
    ERR;
  }

  if (FLAGS_clocks_per_us < 100) {
    cout << "CPU_MHZ is less than 100. are your really?" << endl;
    ERR;
  }

  TMT = new TransactionTable *[FLAGS_thread_num];

  for (unsigned int i = 0; i < FLAGS_thread_num; ++i)
    TMT[i] = new TransactionTable(0, 0);
}

void displayDB() {
  Tuple *tuple;
  Version *version;

  for (unsigned int i = 0; i < FLAGS_tuple_num; ++i) {
    tuple = TxExecutor::get_tuple(Table, i);
    cout << "------------------------------" << endl;  // - 30
    cout << "key: " << i << endl;

    version = tuple->latest_.load(std::memory_order_acquire);
    while (version != NULL) {
      cout << "val_: " << version->val_ << endl;

      switch (version->status_) {
        case VersionStatus::inFlight:
          cout << "status_:  inFlight";
          break;
        case VersionStatus::aborted:
          cout << "status_:  aborted";
          break;
        case VersionStatus::committed:
          cout << "status_:  committed";
          break;
      }
      cout << endl;

      cout << "cstamp_:  " << version->cstamp_ << endl;
      cout << endl;

      version = version->prev_;
    }
    cout << endl;
  }
}

void displayParameter() {
  cout << "#FLAGS_clocks_per_us:\t\t\t" << FLAGS_clocks_per_us << endl;
  cout << "#FLAGS_extime:\t\t\t\t" << FLAGS_extime << endl;
  cout << "#FLAGS_gc_inter_us:\t\t\t" << FLAGS_gc_inter_us << endl;
  cout << "#FLAGS_max_ope:\t\t\t\t" << FLAGS_max_ope << endl;
  cout << "#FLAGS_pre_reserve_tmt_element:\t\t" << FLAGS_pre_reserve_tmt_element
       << endl;
  cout << "#FLAGS_pre_reserve_version:\t\t" << FLAGS_pre_reserve_version
       << endl;
  cout << "#FLAGS_rmw:\t\t\t\t" << FLAGS_rmw << endl;
  cout << "#FLAGS_rratio:\t\t\t\t" << FLAGS_rratio << endl;
  cout << "#FLAGS_thread_num:\t\t\t" << FLAGS_thread_num << endl;
  cout << "#FLAGS_ycsb:\t\t\t\t" << FLAGS_ycsb << endl;
  cout << "#FLAGS_zipf_skew:\t\t\t" << FLAGS_zipf_skew << endl;
}

void partTableInit([[maybe_unused]] size_t thid, uint64_t start, uint64_t end) {
#if MASSTREE_USE
  MasstreeWrapper<Tuple>::thread_init(thid);
#endif

  for (auto i = start; i <= end; ++i) {
    Tuple *tmp;
    Version *verTmp;
    tmp = TxExecutor::get_tuple(Table, i);
    tmp->latest_.store(new Version(), std::memory_order_release);
    // if (posix_memalign((void**)&tmp->latest_, CACHE_LINE_SIZE,
    // sizeof(Version)) != 0) ERR;
    tmp->min_cstamp_ = 0;
    verTmp = tmp->latest_.load(std::memory_order_acquire);
    verTmp->cstamp_ = 0;
    verTmp->prev_ = nullptr;
    verTmp->committed_prev_ = nullptr;
    verTmp->status_.store(VersionStatus::committed, std::memory_order_release);
#if MASSTREE_USE
    MT.insert_value(i, tmp);
#endif
  }
}

void makeDB() {
  if (posix_memalign((void **) &Table, PAGE_SIZE,
                     (FLAGS_tuple_num) * sizeof(Tuple)) != 0)
    ERR;
#if dbs11
  if (madvise((void *)Table, (FLAGS_tuple_num) * sizeof(Tuple),
              MADV_HUGEPAGE) != 0)
    ERR;
#endif

  size_t maxthread = decideParallelBuildNumber(FLAGS_tuple_num);

  std::vector<std::thread> thv;
  for (size_t i = 0; i < maxthread; ++i)
    thv.emplace_back(partTableInit, i, i * (FLAGS_tuple_num / maxthread),
                     (i + 1) * (FLAGS_tuple_num / maxthread) - 1);
  for (auto &th : thv) th.join();
}

void naiveGarbageCollection(const bool &quit) {
  TransactionTable *tmt;

  uint32_t mintxID = UINT32_MAX;
  for (unsigned int i = 1; i < FLAGS_thread_num; ++i) {
    tmt = __atomic_load_n(&TMT[i], __ATOMIC_ACQUIRE);
    mintxID = std::min(mintxID, tmt->txid_.load(std::memory_order_acquire));
  }

  if (mintxID == 0)
    return;
  else {
    // mintxIDから到達不能なバージョンを削除する
    Version *verTmp, *delTarget;
    for (unsigned int i = 0; i < FLAGS_tuple_num; ++i) {
      if (quit) return;

#if MASSTREE_USE
      Tuple *tuple = MT.get_value(i);
#else
      Tuple *tuple = TxExecutor::get_tuple(Table, i);
#endif

      verTmp = tuple->latest_.load(std::memory_order_acquire);
      if (verTmp->status_.load(std::memory_order_acquire) !=
          VersionStatus::committed)
        verTmp = verTmp->committed_prev_;
      // この時点で， verTmp はコミット済み最新バージョン

      uint64_t verCstamp = verTmp->cstamp_.load(std::memory_order_acquire);
      while (mintxID < (verCstamp >> 1)) {
        verTmp = verTmp->committed_prev_;
        if (verTmp == nullptr) break;
        verCstamp = verTmp->cstamp_.load(std::memory_order_acquire);
      }
      if (verTmp == nullptr) continue;
      // verTmp は mintxID によって到達可能．

      // ssn commit protocol によってverTmp->commited_prev_ までアクセスされる．
      verTmp = verTmp->committed_prev_;
      if (verTmp == nullptr) continue;

      // verTmp->prev_ からガベコレ可能
      delTarget = verTmp->prev_;
      if (delTarget == nullptr) continue;

      verTmp->prev_ = nullptr;
      while (delTarget != nullptr) {
        verTmp = delTarget->prev_;
        delete delTarget;
        delTarget = verTmp;
      }
      //-----
    }
  }
}

void leaderWork(GarbageCollection &gcob) {
  if (gcob.chkSecondRange()) {
    gcob.decideGcThreshold();
    gcob.mvSecondRangeToFirstRange();
  }
}

void ShowOptParameters() {
  cout << "#ShowOptParameters() "
       << ": ADD_ANALYSIS " << ADD_ANALYSIS << ": BACK_OFF " << BACK_OFF
       << ": MASSTREE_USE " << MASSTREE_USE << ": KEY_SIZE " << KEY_SIZE
       << ": VAL_SIZE " << VAL_SIZE << endl;
}
