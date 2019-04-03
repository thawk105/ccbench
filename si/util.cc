
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h> // syscall(SYS_gettid),
#include <sys/types.h> // syscall(SYS_gettid), 
#include <unistd.h> // syscall(SYS_gettid), 

#include <atomic>
#include <bitset>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>

#include "../include/check.hpp"
#include "../include/debug.hpp"
#include "../include/random.hpp"
#include "../include/tsc.hpp"
#include "../include/zipf.hpp"

#include "include/common.hpp"
#include "include/procedure.hpp"
#include "include/result.hpp"
#include "include/tuple.hpp"

extern bool chkClkSpan(const uint64_t start, const uint64_t stop, const uint64_t threshold);

void
chkArg(const int argc, const char *argv[])
{
  if (argc != 10) {
  //if (argc != 1) {
    cout << "usage: ./si.exe TUPLE_NUM MAX_OPE THREAD_NUM RRATIO RMW ZIPF_SKEW YCSB CPU_MHZ EXTIME" << endl;
    cout << "example: ./si.exe 200 10 24 50 off 0 off 2400 3" << endl;
    cout << "TUPLE_NUM(int): total numbers of sets of key-value" << endl;
    cout << "MAX_OPE(int): total numbers of operations" << endl;
    cout << "THREAD_NUM(int): total numbers of worker thread" << endl;
    cout << "RRATIO : read ratio [%%]" << endl;
    cout << "RMW : read modify write. on or off." << endl;
    cout << "ZIPF_SKEW : zipf skew. 0 ~ 0.999..." << endl;
    cout << "YCSB : on or off. switch makeProcedure function." << endl;
    cout << "CPU_MHZ(float): your cpuMHz. used by calculate time of yorus 1clock" << endl;
    cout << "EXTIME: execution time [sec]" << endl;

    cout << "Tuple " << sizeof(Tuple) << endl;
    cout << "Version " << sizeof(Version) << endl;
    cout << "uint64_t_64byte " << sizeof(uint64_t_64byte) << endl;
    cout << "TransactionTable " << sizeof(TransactionTable) << endl;
    cout << "KEY_SIZE : " << KEY_SIZE << endl;
    cout << "VAL_SIZE : " << VAL_SIZE << endl;

    exit(0);
  }

  chkInt(argv[1]);
  chkInt(argv[2]);
  chkInt(argv[3]);
  chkInt(argv[4]);
  chkInt(argv[8]);
  chkInt(argv[9]);

  TUPLE_NUM = atoi(argv[1]);
  MAX_OPE = atoi(argv[2]);
  THREAD_NUM = atoi(argv[3]);
  RRATIO = atoi(argv[4]);
  std::string argrmw = argv[5];
  ZIPF_SKEW = atof(argv[6]);
  std::string argst = argv[7];
  CLOCKS_PER_US = atof(argv[8]);
  EXTIME = atoi(argv[9]);

  if (THREAD_NUM < 2) {
    cout << "1 thread is leader thread. \nthread number 1 is no worker thread, so exit." << endl;
    ERR;
  }
  if (RRATIO > 100) {
    cout << "rratio [%%] must be 0 ~ 100" << endl;
    ERR;
  }

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
    TMT = new TransactionTable*[THREAD_NUM];
  } catch (std::bad_alloc) {
    ERR;
  }

  for (unsigned int i = 0; i < THREAD_NUM; ++i)
    TMT[i] = new TransactionTable(0, 0);
}

void
displayDB()
{
  Tuple *tuple;
  Version *version;

  for (unsigned int i = 0; i < TUPLE_NUM; ++i) {
    tuple = &Table[i];
    cout << "------------------------------" << endl; // - 30
    cout << "key: " << i << endl;

    version = tuple->latest.load(std::memory_order_acquire);
    while (version != NULL) {
      cout << "val: " << version->val << endl;

      switch (version->status) {
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

      cout << "cstamp:  " << version->cstamp << endl;
      cout << endl;

      version = version->prev;
    }
    cout << endl;
  }
}

void
displayPRO(Procedure *pro)
{
  for (unsigned int i = 0; i < MAX_OPE; ++i) {
    cout << "(ope, key, val) = (";
    switch (pro[i].ope) {
    case Ope::READ:
        cout << "READ";
        break;
      case Ope::WRITE:
        cout << "WRITE";
      break;
      default:
        break;
    }
    cout << ", " << pro[i].key
      << ", " << pro[i].val << ")" << endl;
  }
}

void
makeDB()
{
  Tuple *tmp;
  Version *verTmp;
  Xoroshiro128Plus rnd;
  rnd.init();

  try {
    if (posix_memalign((void**)&Table, 64, (TUPLE_NUM) * sizeof(Tuple)) != 0) ERR;
    for (unsigned int i = 0; i < TUPLE_NUM; ++i) {
      if (posix_memalign((void**)&Table[i].latest, 64, sizeof(Version)) != 0) ERR;
    }
  } catch (std::bad_alloc) {
    ERR;
  }

  for (unsigned int i = 0; i < TUPLE_NUM; ++i) {
    tmp = &Table[i];
    tmp->min_cstamp = 0;
    verTmp = tmp->latest.load(std::memory_order_acquire);
    verTmp->cstamp = 0;
    verTmp->prev = nullptr;
    verTmp->committed_prev = nullptr;
    verTmp->status.store(VersionStatus::committed, std::memory_order_release);
  }
}

void
makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd)
{
  for (unsigned int i = 0; i < MAX_OPE; ++i) {
    if ((rnd.next() % 100) < RRATIO)
      pro[i].ope = Ope::READ;
    else
      pro[i].ope = Ope::WRITE;
    
    pro[i].key = rnd.next() % TUPLE_NUM;
  }
}

void 
makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd, FastZipf &zipf)
{
  for (unsigned int i = 0; i < MAX_OPE; ++i) {
    if ((rnd.next() % 100) < RRATIO) 
      pro[i].ope = Ope::READ;
    else
      pro[i].ope = Ope::WRITE;
    
    pro[i].key = zipf() % TUPLE_NUM;
  }
}

void
naiveGarbageCollection(SIResult &res) 
{
  TransactionTable *tmt;

  uint32_t mintxID = UINT32_MAX;
  for (unsigned int i = 1; i < THREAD_NUM; ++i) {
    tmt = __atomic_load_n(&TMT[i], __ATOMIC_ACQUIRE);
    mintxID = std::min(mintxID, tmt->txid.load(std::memory_order_acquire));
  }

  if (mintxID == 0) return;
  else {
    //mintxIDから到達不能なバージョンを削除する
    Version *verTmp, *delTarget;
    for (unsigned int i = 0; i < TUPLE_NUM; ++i) {
      // 時間がかかるので，離脱条件チェック
      res.end = rdtsc();
      if (chkClkSpan(res.bgn, res.end, EXTIME * 1000 * 1000 * CLOCKS_PER_US)) {
        Finish.store(true, std::memory_order_release);
        return;
      }
      // -----

      verTmp = Table[i].latest.load(std::memory_order_acquire);
      if (verTmp->status.load(std::memory_order_acquire) != VersionStatus::committed) 
        verTmp = verTmp->committed_prev;
      // この時点で， verTmp はコミット済み最新バージョン

      uint64_t verCstamp = verTmp->cstamp.load(std::memory_order_acquire);
      while (mintxID < (verCstamp >> 1)) {
        verTmp = verTmp->committed_prev;
        if (verTmp == nullptr) break;
        verCstamp = verTmp->cstamp.load(std::memory_order_acquire);
      }
      if (verTmp == nullptr) continue;
      // verTmp は mintxID によって到達可能．
      
      // ssn commit protocol によってverTmp->commited_prev までアクセスされる．
      verTmp = verTmp->committed_prev;
      if (verTmp == nullptr) continue;
      
      // verTmp->prev からガベコレ可能
      delTarget = verTmp->prev;
      if (delTarget == nullptr) continue;

      verTmp->prev = nullptr;
      while (delTarget != nullptr) {
        verTmp = delTarget->prev;
        delete delTarget;
        delTarget = verTmp;
      }
      //-----
    }
  }
}

