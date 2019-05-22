
#include <stdlib.h>
#include <sys/syscall.h> // syscall(SYS_gettid),
#include <sys/types.h> // syscall(SSY_gettid),
#include <unistd.h> // syscall(SSY_gettid),

#include <atomic>
#include <bitset>
#include <cstdint>
#include <thread>
#include <iomanip>
#include <iostream>
#include <limits>
#include <vector>

#include "include/common.hpp"
#include "include/procedure.hpp"
#include "include/tuple.hpp"

#include "../include/check.hpp"
#include "../include/debug.hpp"
#include "../include/masstree_wrapper.hpp"
#include "../include/random.hpp"
#include "../include/zipf.hpp"

extern size_t decide_parallel_build_number(size_t tuplenum);

void
chkArg(const int argc, const char *argv[])
{
  if (argc != 10) {
    cout << "usage: ./ss2pl.exe TUPLE_NUM MAX_OPE THREAD_NUM RRATIO RMW ZIPF_SKEW YCSB CPU_MHZ EXTIME" << endl << endl;
    cout << "example: ./ss2pl.exe 200 10 24 50 off 0 on 2400 3" << endl << endl;
    cout << "TUPLE_NUM(int): total numbers of sets of key-value" << endl;
    cout << "MAX_OPE(int): total numbers of operations" << endl;
    cout << "THREAD_NUM(int): total numbers of worker thread" << endl;
    cout << "RRATIO : read ratio [%%]" << endl;
    cout << "RMW : read modify write. on or off." << endl;
    cout << "ZIPF_SKEW : zipf skew. 0 ~ 0.999..." << endl;

    cout << "YCSB : on or off. switch makeProcedure function." << endl;
    cout << "CPU_MHZ(float): your cpuMHz. used by calculate time of yorus 1clock" << endl;
    cout << "EXTIME: execution time [sec]" << endl << endl;

    cout << "Tuple size " << sizeof(Tuple) << endl;
    cout << "std::mutex size " << sizeof(mutex) << endl;
    cout << "RWLock size " << sizeof(RWLock) << endl;
    cout << "KEY_SIZE : " << KEY_SIZE << endl;
    cout << "VAL_SIZE : " << VAL_SIZE << endl;
    cout << "std::thread::hardware_concurrency()=" << std::thread::hardware_concurrency() << endl;
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
  string argrmw = argv[5];
  ZIPF_SKEW = atof(argv[6]);
  string argycsb = argv[7];
  CLOCKS_PER_US = atof(argv[8]);
  EXTIME = atoi(argv[9]);

  if (RRATIO > 100) {
    ERR;
  }

  if (argrmw == "on")
    RMW = true;
  else if (argrmw == "off")
    RMW = false;
  else
    ERR;

  if (argycsb == "on")
    YCSB = true;
  else if (argycsb == "off")
    YCSB = false;
  else
    ERR;

  if (CLOCKS_PER_US < 100) {
    cout << "CPU_MHZ is less than 100. are your really?" << endl;
    ERR;
  }
}

void
displayDB()
{
  Tuple *tuple;

  for (unsigned int i = 0; i < TUPLE_NUM; i++) {
    tuple = &Table[i];
    cout << "------------------------------" << endl; // - 30
    cout << "key: " << i << endl;
    cout << "val: " << tuple->val << endl;
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
part_table_init([[maybe_unused]]size_t thid, uint64_t start, uint64_t end)
{
  //printf("part_table_init(...): thid %zu : %lu : %lu\n", thid, start, end);
#if MASSTREE_USE
  MasstreeWrapper<Tuple>::thread_init(thid);
#endif

  for (auto i = start; i <= end; ++i) {
    Table[i].val[0] = 'a';
    Table[i].val[1] = '\0';

#if MASSTREE_USE
    MT.insert_value(i, &Table[i]);
#endif
  }
}

void
makeDB()
{
  try {
    if (posix_memalign((void**)&Table, 64, TUPLE_NUM * sizeof(Tuple)) != 0) ERR;
  } catch (bad_alloc) {
    ERR;
  }


  // maxthread は masstree 構築の最大並行スレッド数。
  // 初期値はハードウェア最大値。
  // TUPLE_NUM を均等に分割できる最大スレッド数を求める。
  size_t maxthread = decide_parallel_build_number(TUPLE_NUM);

  std::vector<std::thread> thv;
  //cout << "masstree 並列構築スレッド数 " << maxthread << endl;
  for (size_t i = 0; i < maxthread; ++i) {
    thv.emplace_back(part_table_init, i, i * (TUPLE_NUM / maxthread), (i + 1) * (TUPLE_NUM / maxthread) - 1);
  }
  for (auto& th : thv) th.join();
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
makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd, FastZipf &zipf) {
  for (unsigned int i = 0; i < MAX_OPE; ++i) {
    if ((rnd.next() % 100) < RRATIO)
      pro[i].ope = Ope::READ;
    else
      pro[i].ope = Ope::WRITE;

    pro[i].key = zipf() % TUPLE_NUM;
  }
}

