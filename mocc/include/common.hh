#pragma once

#include <atomic>

#include "tuple.hh"

#include "../../include/cache_line_size.hh"
#include "../../include/config.hh"
#include "../../include/int64byte.hh"
#include "../../include/masstree_wrapper.hh"
#include "../../include/random.hh"

#include "gflags/gflags.h"
#include "glog/logging.h"

#ifdef GLOBAL_VALUE_DEFINE
#define GLOBAL
alignas(CACHE_LINE_SIZE) GLOBAL uint64_t_64byte GlobalEpoch(1);
#if MASSTREE_USE
alignas(CACHE_LINE_SIZE) GLOBAL MasstreeWrapper<Tuple> MT;
#endif
#else
#define GLOBAL extern
alignas(CACHE_LINE_SIZE) GLOBAL uint64_t_64byte GlobalEpoch;
#if MASSTREE_USE
alignas(CACHE_LINE_SIZE) GLOBAL MasstreeWrapper<Tuple> MT;
#endif
#endif

alignas(CACHE_LINE_SIZE) GLOBAL uint64_t_64byte *ThLocalEpoch;

#ifdef GLOBAL_VALUE_DEFINE
DEFINE_uint64(clocks_per_us, 2100,
              "CPU_MHz. Use this info for measuring time.");
DEFINE_uint64(epoch_time, 40, "Epoch interval[msec].");
DEFINE_uint64(extime, 3, "Execution time[sec].");
DEFINE_uint64(max_ope, 10,
              "Total number of operations per single transaction.");
DEFINE_uint64(per_xx_temp, 4096, "What record size (bytes) does it integrate about temperature statistics.");
DEFINE_bool(rmw, false,
            "True means read modify write, false means blind write.");
DEFINE_uint64(rratio, 50, "read ratio of single transaction.");
DEFINE_uint64(thread_num, 10, "Total number of worker threads.");
DEFINE_uint64(tuple_num, 1000000, "Total number of records.");
DEFINE_bool(ycsb, true,
            "True uses zipf_skew, false uses faster random generator.");
DEFINE_double(zipf_skew, 0, "zipf skew. 0 ~ 0.999...");
#else
DECLARE_uint64(clocks_per_us);
DECLARE_uint64(epoch_time);
DECLARE_uint64(extime);
DECLARE_uint64(max_ope);
DECLARE_uint64(per_xx_temp);
DECLARE_bool(rmw);
DECLARE_uint64(rratio);
DECLARE_uint64(thread_num);
DECLARE_uint64(tuple_num);
DECLARE_bool(ycsb);
DECLARE_double(zipf_skew);
#endif

GLOBAL RWLock CtrLock;
// temperature, min 0, max 20
alignas(PAGE_SIZE) GLOBAL Epotemp *EpotempAry;

// for logging emulation
alignas(CACHE_LINE_SIZE) GLOBAL uint64_t_64byte *Start;
alignas(CACHE_LINE_SIZE) GLOBAL uint64_t_64byte *Stop;

alignas(CACHE_LINE_SIZE) GLOBAL Tuple *Table;

// 下記，ifdef で外すと lock.cc のコードから
// 参照出来なくてエラーが起きる．
// lock.hh, lock.cc の全てを ifdef で分岐させるのは大変な労力なので，
// 行わず，これは RWLOCK モードでも宣言だけしておく．
alignas(CACHE_LINE_SIZE) GLOBAL MQLNode **MQLNodeTable;
// first dimension index number corresponds to the thread number.
// second dimension index number corresponds to the key of records.
// the element mean MQLnode whihch is owned by the thread which has the thread
// number corresponding to index number. for MQL sentinel value index 0 mean
// None. index 1 mean Acquired. index 2 mean SuccessorLeaving.
