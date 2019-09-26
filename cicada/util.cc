#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>  // syscall(SYS_gettid),
#include <sys/time.h>
#include <sys/types.h>  // syscall(SYS_gettid),
#include <unistd.h>     // syscall(SYS_gettid),
#include <atomic>
#include <bitset>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>

#include "../include/backoff.hh"
#include "../include/cache_line_size.hh"
#include "../include/check.hh"
#include "../include/config.hh"
#include "../include/debug.hh"
#include "../include/masstree_wrapper.hh"
#include "../include/random.hh"
#include "../include/zipf.hh"
#include "include/common.hh"
#include "include/time_stamp.hh"
#include "include/transaction.hh"
#include "include/tuple.hh"

using std::cout, std::endl;

extern size_t decideParallelBuildNumber(size_t tuplenum);

void chkArg(const int argc, char *argv[]) {
  if (argc != 16) {
    cout << "usage: ./cicada.exe TUPLE_NUM MAX_OPE THREAD_NUM RRATIO RMW "
            "ZIPF_SKEW YCSB WAL GROUP_COMMIT CPU_MHZ IO_TIME_NS "
            "GROUP_COMMIT_TIMEOUT_US GC_INTER_US PRE_RESERVE_VERSION EXTIME"
         << endl
         << endl;
    cout << "example:./main 200 10 24 50 off 0 on off off 2100 5 2 10 10000 3"
         << endl
         << endl;
    cout << "TUPLE_NUM(int): total numbers of sets of key-value (1, 100), (2, "
            "100)"
         << endl;
    cout << "MAX_OPE(int):    total numbers of operations" << endl;
    cout << "THREAD_NUM(int): total numbers of worker thread." << endl;
    cout << "RRATIO: read ratio [%%]" << endl;
    cout << "RMW : read modify write. on or off." << endl;
    cout << "ZIPF_SKEW : zipf skew. 0 ~ 0.999..." << endl;
    cout << "YCSB : on or off. switch makeProcedure function." << endl;
    cout << "WAL: P or S or off." << endl;
    cout << "GROUP_COMMIT:  unsigned integer or off, i reccomend off or 3"
         << endl;
    cout << "CPU_MHZ(float):  your cpuMHz. used by calculate time of yours "
            "1clock."
         << endl;
    cout << "IO_TIME_NS: instead of exporting to disk, delay is inserted. the "
            "time(nano seconds)."
         << endl;
    cout << "GROUP_COMMIT_TIMEOUT_US: Invocation condition of group commit by "
            "timeout(micro seconds)."
         << endl;
    cout << "GC_INTER_US: garbage collection interval [usec]" << endl;
    cout << "PRE_RESERVE_VERSION: pre-prepare memory for version generation."
         << endl;
    cout << "EXTIME: execution time [sec]" << endl << endl;

    cout << "Tuple " << sizeof(Tuple) << endl;
    cout << "Version " << sizeof(Version) << endl;
    cout << "TimeStamp " << sizeof(TimeStamp) << endl;
    cout << "Procedure " << sizeof(Procedure) << endl;
    cout << "KEY_SIZE : " << KEY_SIZE << endl;
    cout << "VAL_SIZE : " << VAL_SIZE << endl;
    cout << "CACHE_LINE_SIZE - ((17 + KEY_SIZE + sizeof(Version)) % "
            "(CACHE_LINE_SIZE)) : "
         << CACHE_LINE_SIZE -
                ((17 + KEY_SIZE + sizeof(Version)) % (CACHE_LINE_SIZE))
         << endl;
    cout << "CACHE_LINE_SIZE - ((25 + VAL_SIZE) % (CACHE_LINE_SIZE)) : "
         << CACHE_LINE_SIZE - ((25 + VAL_SIZE) % (CACHE_LINE_SIZE)) << endl;
    cout << "MASSTREE_USE : " << MASSTREE_USE << endl;
    cout << "Result:\t" << sizeof(Result) << endl;
    cout << "uint64_t_64byte: " << sizeof(uint64_t_64byte) << endl;
    exit(0);
  }

  chkInt(argv[1]);
  chkInt(argv[2]);
  chkInt(argv[3]);
  chkInt(argv[4]);
  chkInt(argv[10]);
  chkInt(argv[11]);
  chkInt(argv[12]);
  chkInt(argv[13]);
  chkInt(argv[14]);
  chkInt(argv[15]);

  TUPLE_NUM = atoi(argv[1]);
  MAX_OPE = atoi(argv[2]);
  THREAD_NUM = atoi(argv[3]);
  RRATIO = atoi(argv[4]);
  string argrmw = argv[5];
  ZIPF_SKEW = atof(argv[6]);
  string argycsb = argv[7];
  string argwal = argv[8];
  string arggrpc = argv[9];
  CLOCKS_PER_US = atof(argv[10]);
  IO_TIME_NS = atof(argv[11]);
  GROUP_COMMIT_TIMEOUT_US = atoi(argv[12]);
  GC_INTER_US = atoi(argv[13]);
  PRE_RESERVE_VERSION = atoi(argv[14]);
  EXTIME = atoi(argv[15]);

  if (RRATIO > 100) {
    cout << "rratio [%%] must be 0 ~ 100)" << endl;
    ERR;
  }

  if (ZIPF_SKEW >= 1) {
    cout << "ZIPF_SKEW must be 0 ~ 0.999..." << endl;
    ERR;
  }

  if (argrmw == "on")
    RMW = true;
  else if (argrmw == "off")
    RMW = false;
  else
    ERR;

  if (argycsb == "on") {
    YCSB = true;
  } else if (argycsb == "off") {
    YCSB = false;
  } else
    ERR;

  if (argwal == "P") {
    P_WAL = true;
    S_WAL = false;
  } else if (argwal == "S") {
    P_WAL = false;
    S_WAL = true;
  } else if (argwal == "off") {
    P_WAL = false;
    S_WAL = false;

    if (arggrpc != "off") {
      printf(
          "i don't implement below.\n\
P_WAL off, S_WAL off, GROUP_COMMIT number.\n\
usage: P_WAL or S_WAL is selected. \n\
P_WAL and S_WAL isn't selected, GROUP_COMMIT must be off. this isn't logging. performance is concurrency control only.\n\n");
      exit(0);
    }
  } else {
    printf("WAL must be P or S or off\n");
    exit(0);
  }

  if (arggrpc == "off")
    GROUP_COMMIT = 0;
  else if (chkInt(argv[9])) {
    GROUP_COMMIT = atoi(argv[9]);
  } else {
    printf("GROUP_COMMIT(argv[9]) must be unsigned integer or off\n");
    exit(0);
  }

  if (CLOCKS_PER_US < 100) {
    printf("CPU_MHZ is less than 100. are you really?\n");
    exit(0);
  }

  if (posix_memalign((void **)&ThreadRtsArrayForGroup, CACHE_LINE_SIZE,
                     THREAD_NUM * sizeof(uint64_t_64byte)) != 0)
    ERR;
  if (posix_memalign((void **)&ThreadWtsArray, CACHE_LINE_SIZE,
                     THREAD_NUM * sizeof(uint64_t_64byte)) != 0)
    ERR;
  if (posix_memalign((void **)&ThreadRtsArray, CACHE_LINE_SIZE,
                     THREAD_NUM * sizeof(uint64_t_64byte)) != 0)
    ERR;
  if (posix_memalign((void **)&GROUP_COMMIT_INDEX, CACHE_LINE_SIZE,
                     THREAD_NUM * sizeof(uint64_t_64byte)) != 0)
    ERR;
  if (posix_memalign((void **)&GROUP_COMMIT_COUNTER, CACHE_LINE_SIZE,
                     THREAD_NUM * sizeof(uint64_t_64byte)) != 0)
    ERR;
  if (posix_memalign((void **)&GCFlag, CACHE_LINE_SIZE,
                     THREAD_NUM * sizeof(uint64_t_64byte)) != 0)
    ERR;
  if (posix_memalign((void **)&GCExecuteFlag, CACHE_LINE_SIZE,
                     THREAD_NUM * sizeof(uint64_t_64byte)) != 0)
    ERR;

  SLogSet = new Version *[(MAX_OPE) * (GROUP_COMMIT)];
  PLogSet = new Version **[THREAD_NUM];

  for (unsigned int i = 0; i < THREAD_NUM; ++i) {
    PLogSet[i] = new Version *[(MAX_OPE) * (GROUP_COMMIT)];
  }

  // init
  for (unsigned int i = 0; i < THREAD_NUM; ++i) {
    GCFlag[i].obj_ = 0;
    GCExecuteFlag[i].obj_ = 0;
    GROUP_COMMIT_INDEX[i].obj_ = 0;
    GROUP_COMMIT_COUNTER[i].obj_ = 0;
    ThreadRtsArray[i].obj_ = 0;
    ThreadWtsArray[i].obj_ = 0;
    ThreadRtsArrayForGroup[i].obj_ = 0;
  }
}

void displayDB() {
  Tuple *tuple;
  Version *version;

  for (unsigned int i = 0; i < TUPLE_NUM; ++i) {
    tuple = &Table[i % TUPLE_NUM];
    cout << "------------------------------" << endl;  //-は30個
    cout << "key: " << i << endl;

    version = tuple->latest_;
    while (version != NULL) {
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

void displayMinRts() { cout << "MinRts:  " << MinRts << endl << endl; }

void displayMinWts() { cout << "MinWts:  " << MinWts << endl << endl; }

void displayThreadWtsArray() {
  cout << "ThreadWtsArray:" << endl;
  for (unsigned int i = 0; i < THREAD_NUM; i++) {
    cout << "thid " << i << ": " << ThreadWtsArray[i].obj_ << endl;
  }
  cout << endl << endl;
}

void displayThreadRtsArray() {
  cout << "ThreadRtsArray:" << endl;
  for (unsigned int i = 0; i < THREAD_NUM; i++) {
    cout << "thid " << i << ": " << ThreadRtsArray[i].obj_ << endl;
  }
  cout << endl << endl;
}

void displaySLogSet() {
  if (!GROUP_COMMIT) {
  } else {
    if (S_WAL) {
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

void partTableInit([[maybe_unused]] size_t thid, uint64_t initts,
                   uint64_t start, uint64_t end) {
#if MASSTREE_USE
  MasstreeWrapper<Tuple>::thread_init(thid);
#endif

  for (uint64_t i = start; i <= end; ++i) {
    Tuple *tuple;
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
    Tuple *tuple;
    tuple = TxExecutor::get_tuple(Table, i);
    Version *ver = tuple->latest_;
    while (ver != nullptr) {
#if INLINE_VERSION_OPT
      if (ver == &tuple->inline_ver_) {
        ver = ver->next_.load(memory_order_acquire);
        continue;
      }
#endif
      Version *del = ver;
      ver = ver->next_.load(memory_order_acquire);
      delete del;
    }
  }
}

void deleteDB() {
  size_t maxthread = decideParallelBuildNumber(TUPLE_NUM);
  std::vector<std::thread> thv;
  for (size_t i = 0; i < maxthread; ++i)
    thv.emplace_back(partTableDelete, i, i * (TUPLE_NUM / maxthread),
                     (i + 1) * (TUPLE_NUM / maxthread) - 1);
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
  for (uint i = 0; i < THREAD_NUM; ++i) delete PLogSet[i];
  delete PLogSet;
}

void makeDB(uint64_t *initial_wts) {
  if (posix_memalign((void **)&Table, PAGE_SIZE, TUPLE_NUM * sizeof(Tuple)) !=
      0)
    ERR;
#if dbs11
  if (madvise((void *)Table, (TUPLE_NUM) * sizeof(Tuple), MADV_HUGEPAGE) != 0)
    ERR;
#endif

  TimeStamp tstmp;
  tstmp.generateTimeStampFirst(0);
  *initial_wts = tstmp.ts_;

  size_t maxthread = decideParallelBuildNumber(TUPLE_NUM);
  std::vector<std::thread> thv;
  for (size_t i = 0; i < maxthread; ++i)
    thv.emplace_back(partTableInit, i, tstmp.ts_, i * (TUPLE_NUM / maxthread),
                     (i + 1) * (TUPLE_NUM / maxthread) - 1);
  for (auto &th : thv) th.join();
}

void leaderWork([[maybe_unused]] Backoff &backoff,
                [[maybe_unused]] std::vector<Result> &res) {
  bool gc_update = true;
  for (unsigned int i = 0; i < THREAD_NUM; ++i) {
    // check all thread's flag raising
    if (__atomic_load_n(&(GCFlag[i].obj_), __ATOMIC_ACQUIRE) == 0) {
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
      minr =
          __atomic_load_n(&(ThreadRtsArrayForGroup[1].obj_), __ATOMIC_ACQUIRE);
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
    for (unsigned int i = 0; i < THREAD_NUM; ++i) {
      __atomic_store_n(&(GCFlag[i].obj_), 0, __ATOMIC_RELEASE);
      __atomic_store_n(&(GCExecuteFlag[i].obj_), 1, __ATOMIC_RELEASE);
    }
  }

#if BACK_OFF
  leaderBackoffWork(backoff, res);
#endif
}
