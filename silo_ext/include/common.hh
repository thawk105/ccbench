#pragma once

#include <pthread.h>
#include <atomic>
#include <iostream>
#include <queue>

//#include "tuple.hh"

#include "../../include/cache_line_size.hh"
#include "../../include/int64byte.hh"
//#include "../../include/masstree_wrapper.hh"

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

#ifdef GLOBAL_VALUE_DEFINE
DEFINE_uint64(thread_num, 1, "Total number of worker threads.");
DEFINE_uint64(clocks_per_us, 2100,
              "CPU_MHz. Use this info for measuring time.");
DEFINE_uint64(epoch_time, 40, "Epoch interval[msec].");
DEFINE_uint64(extime, 1, "Execution time[sec].");

DEFINE_uint32(num_wh, 1, "The number of warehouses");
DEFINE_double(perc_payment, 50, "The percentage of Payment transactions"); // 43.1 for full
DEFINE_double(perc_order_status, 0, "The percentage of Order-Status transactions"); // 4.1 for full
DEFINE_double(perc_delivery, 0, "The percentage of Delivery transactions"); // 4.2 for full
DEFINE_double(perc_stock_level, 0, "The percentage of Stock-Level transactions"); // 4.1 for full

#else
DECLARE_uint64(thread_num);
DECLARE_uint64(clocks_per_us);
DECLARE_uint64(epoch_time);
DECLARE_uint64(extime);

DECLARE_uint32(num_wh);
DECLARE_double(perc_payment);
DECLARE_double(perc_order_status);
DECLARE_double(perc_delivery);
DECLARE_double(perc_stock_level);
#endif

constexpr std::size_t DIST_PER_WARE{10};
constexpr std::size_t MAX_ITEMS{100000};
constexpr std::size_t CUST_PER_DIST{3000};


alignas(CACHE_LINE_SIZE) GLOBAL uint64_t_64byte *ThLocalEpoch;
alignas(CACHE_LINE_SIZE) GLOBAL uint64_t_64byte *CTIDW;

//alignas(CACHE_LINE_SIZE) GLOBAL Tuple *Table;
