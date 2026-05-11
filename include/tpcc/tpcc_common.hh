#pragma once

#include "gflags/gflags.h"

#ifdef GLOBAL_VALUE_DEFINE
DEFINE_uint32(tpcc_num_wh, 1, "The number of warehouses");
DEFINE_uint64(tpcc_perc_payment, 43, "The percentage of Payment transactions");
DEFINE_uint64(tpcc_perc_order_status, 4, "The percentage of Order-Status transactions");
DEFINE_uint64(tpcc_perc_delivery, 4, "The percentage of Delivery transactions");
DEFINE_uint64(tpcc_perc_stock_level, 4, "The percentage of Stock-Level transactions");
DEFINE_uint32(tpcc_interactive_ms, 0, "Sleep milliseconds per SQL(-equivalent) unit");
#else
DECLARE_uint32(tpcc_num_wh);
DECLARE_uint64(tpcc_perc_payment);
DECLARE_uint64(tpcc_perc_order_status);
DECLARE_uint64(tpcc_perc_delivery);
DECLARE_uint64(tpcc_perc_stock_level);
DECLARE_uint64(tpcc_interactive_ms);
#endif

constexpr std::size_t DIST_PER_WARE{10};
constexpr std::size_t MAX_ITEMS{100000};
constexpr std::size_t CUST_PER_DIST{3000};
constexpr std::size_t LASTNAME_LEN{16};
