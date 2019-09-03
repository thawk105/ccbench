
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

#include "../include/cache_line_size.hh"
#include "../include/check.hh"
#include "../include/config.hh"
#include "../include/debug.hh"
#include "../include/masstree_wrapper.hh"
#include "../include/random.hh"
#include "../include/result.hh"
#include "../include/tsc.hh"
#include "../include/zipf.hh"
#include "include/common.hh"
#include "include/tuple.hh"

extern bool chkClkSpan(const uint64_t start, const uint64_t stop,
                       const uint64_t threshold);
extern size_t decideParallelBuildNumber(size_t tuplenum);

void chkArg(const int argc, const char *argv[]) {
  if (argc != 13) {
    // if (argc != 1) {
    cout << "usage: ./ermia.exe TUPLE_NUM MAX_OPE THREAD_NUM RRATIO RMW "
            "ZIPF_SKEW YCSB CPU_MHZ GC_INTER_US PRE_RESERVE_TMT_ELEMENT "
            "PRE_RESERVE_VERSION EXTIME"
         << endl;
    cout << "example: ./ermia.exe 200 10 24 50 off 0 off 2100 10 1000 10000 3"
         << endl;
    cout << "TUPLE_NUM(int): total numbers of sets of key-value" << endl;
    cout << "MAX_OPE(int): total numbers of operations" << endl;
    cout << "THREAD_NUM(int): total numbers of worker thread" << endl;
    cout << "RRATIO : read ratio [%%]" << endl;
    cout << "RMW : read modify write. on or off" << endl;
    cout << "ZIPF_SKEW : zipf skew. 0 ~ 0.999..." << endl;
    cout << "YCSB : on or off. switch makeProcedure function." << endl;
    cout
        << "CPU_MHZ(float): your cpuMHz. used by calculate time of yorus 1clock"
        << endl;
    cout << "GC_INTER_US : garbage collection interval [usec]" << endl;
    cout
        << "PRE_RESERVE_TMT_ELEMENT: pre-prepare memory for version generation."
        << endl;
    cout << "PRE_RESERVE_VERSION: pre-prepare memory for version generation."
         << endl;
    cout << "EXTIME: execution time [sec]" << endl;

    cout << "Tuple " << sizeof(Tuple) << endl;
    cout << "Version " << sizeof(Version) << endl;
    cout << "uint64_t_64byte " << sizeof(uint64_t_64byte) << endl;
    cout << "TransactionTable " << sizeof(TransactionTable) << endl;
    cout << "KEY_SIZE : " << KEY_SIZE << endl;
    cout << "VAL_SIZE : " << VAL_SIZE << endl;
    cout << "MASSTREE_USE : " << MASSTREE_USE << endl;
    exit(0);
  }

  chkInt(argv[1]);
  chkInt(argv[2]);
  chkInt(argv[3]);
  chkInt(argv[4]);
  chkInt(argv[8]);
  chkInt(argv[9]);
  chkInt(argv[10]);
  chkInt(argv[11]);

  TUPLE_NUM = atoi(argv[1]);
  MAX_OPE = atoi(argv[2]);
  THREAD_NUM = atoi(argv[3]);
  RRATIO = atoi(argv[4]);
  string argrmw = argv[5];
  ZIPF_SKEW = atof(argv[6]);
  string argst = argv[7];
  CLOCKS_PER_US = atof(argv[8]);
  GC_INTER_US = atoi(argv[9]);
  PRE_RESERVE_TMT_ELEMENT = atoi(argv[10]);
  PRE_RESERVE_VERSION = atoi(argv[11]);
  EXTIME = atoi(argv[12]);

  if (THREAD_NUM < 1) {
    cout << "1 is minimum thread number."
            "so exit."
         << endl;
    ERR;
  }

  if (RRATIO > 100) {
    cout << "rratio [%%] must be 0 ~ 100" << endl;
    ERR;
  }

  if (argrmw == "on")
    RMW = true;
  else if (argrmw == "off")
    RMW = false;
  else
    ERR;

  if (ZIPF_SKEW >= 1) {
    cout << "ZIPF_SKEW must be 0 ~ 0.999..." << endl;
    ERR;
  }

  if (argst == "on")
    YCSB = true;
  else if (argst == "off")
    YCSB = false;
  else
    ERR;

  if (CLOCKS_PER_US < 100) {
    cout << "CPU_MHZ is less than 100. are your really?" << endl;
    ERR;
  }

  try {
    TMT = new TransactionTable *[THREAD_NUM];
  } catch (bad_alloc) {
    ERR;
  }

  for (unsigned int i = 0; i < THREAD_NUM; ++i) {
    TMT[i] =
        new TransactionTable(0, 0, UINT32_MAX, 0, TransactionStatus::inFlight);
  }
}

void displayDB() {
  Tuple *tuple;
  Version *version;

  for (unsigned int i = 0; i < TUPLE_NUM; ++i) {
    tuple = &Table[i];
    cout << "------------------------------" << endl;  // - 30
    cout << "key: " << i << endl;

    version = tuple->latest_.load(std::memory_order_acquire);
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
      cout << "pstamp:  " << version->psstamp_.pstamp_ << endl;
      cout << "sstamp:  " << version->psstamp_.sstamp_ << endl;
      cout << endl;

      version = version->prev_;
    }
    cout << endl;
  }
}

void partTableInit([[maybe_unused]] size_t thid, uint64_t start, uint64_t end) {
#if MASSTREE_USE
  MasstreeWrapper<Tuple>::thread_init(thid);
#endif

  for (uint64_t i = start; i <= end; ++i) {
    Tuple *tmp;
    tmp = &Table[i];
    tmp->min_cstamp_ = 0;
    if (posix_memalign((void **)&tmp->latest_, CACHE_LINE_SIZE,
                       sizeof(Version)) != 0)
      ERR;
    Version *verTmp = tmp->latest_.load(std::memory_order_acquire);
    verTmp->cstamp_ = 0;
    // verTmp->pstamp = 0;
    // verTmp->sstamp = UINT64_MAX & ~(1);
    verTmp->psstamp_.pstamp_ = 0;
    verTmp->psstamp_.sstamp_ = UINT32_MAX & ~(1);
    // cstamp, sstamp の最下位ビットは TID フラグ
    // 1の時はTID, 0の時はstamp
    verTmp->prev_ = nullptr;
    verTmp->status_.store(VersionStatus::committed, std::memory_order_release);
    verTmp->readers_.store(0, std::memory_order_release);
#if MASSTREE_USE
    MT.insert_value(i, tmp);
#endif
  }
}

void makeDB() {
  if (posix_memalign((void **)&Table, PAGE_SIZE, (TUPLE_NUM) * sizeof(Tuple)) !=
      0)
    ERR;
#if dbs11
  if (madvise((void *)Table, (TUPLE_NUM) * sizeof(Tuple), MADV_HUGEPAGE) != 0)
    ERR;
#endif

  size_t maxthread = decideParallelBuildNumber(TUPLE_NUM);
  std::vector<std::thread> thv;
  for (size_t i = 0; i < maxthread; ++i)
    thv.emplace_back(partTableInit, i, i * (TUPLE_NUM / maxthread),
                     (i + 1) * (TUPLE_NUM / maxthread) - 1);
  for (auto &th : thv) th.join();
}

void naiveGarbageCollection(const bool &quit) {
  TransactionTable *tmt;

  uint32_t mintxID = UINT32_MAX;
  for (unsigned int i = 1; i < THREAD_NUM; ++i) {
    tmt = __atomic_load_n(&TMT[i], __ATOMIC_ACQUIRE);
    mintxID = min(mintxID, tmt->txid_.load(std::memory_order_acquire));
  }

  if (mintxID == 0)
    return;
  else {
    // mintxIDから到達不能なバージョンを削除する
    Version *verTmp, *delTarget;
    for (unsigned int i = 0; i < TUPLE_NUM; ++i) {
      // 時間がかかるので，離脱条件チェック
      if (quit == true) return;

      verTmp = Table[i].latest_.load(memory_order_acquire);
      while (verTmp->status_.load(memory_order_acquire) !=
             VersionStatus::committed)
        verTmp = verTmp->prev_;
      // この時点で， verTmp はコミット済み最新バージョン

      uint64_t verCstamp = verTmp->cstamp_.load(memory_order_acquire);
      while (mintxID < (verCstamp >> 1) ||
             verTmp->status_.load(memory_order_acquire) !=
                 VersionStatus::committed) {
        verTmp = verTmp->prev_;
        if (verTmp == nullptr) break;
        verCstamp = verTmp->cstamp_.load(memory_order_acquire);
      }
      if (verTmp == nullptr) continue;
      // verTmp は mintxID によって到達可能．

      // ssn commit protocol によってverTmp->commited_prev までアクセスされる．
      verTmp = verTmp->prev_;
      while (verTmp->status_.load(memory_order_acquire) !=
             VersionStatus::committed)
        verTmp = verTmp->prev_;
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
