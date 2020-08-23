/**
 * @file tpcc_query.hpp
 */

#pragma once

#include "../include/common.hh"
#include "../include/random.hh"
#include "../include/result.hh"

#define TPCC_SMALL  false
#define LASTNAME_LEN  16

namespace TPCC {

enum QueryType {
  Q_NONE,
  Q_NEW_ORDER,
  Q_PAYMENT,
  Q_ORDER_STATUS,
  Q_DELIVERY,
  Q_STOCK_LEVEL
};

namespace query {

class Option {
public:
  std::uint32_t num_wh = FLAGS_num_wh;
  std::uint32_t dist_per_ware = DIST_PER_WARE;
  std::uint32_t max_items = MAX_ITEMS;
  std::uint32_t cust_per_dist = CUST_PER_DIST;
  std::uint64_t perc_payment = FLAGS_perc_payment;
  std::uint64_t perc_order_status = FLAGS_perc_order_status;
  std::uint64_t perc_delivery = FLAGS_perc_delivery;
  std::uint64_t perc_stock_level = FLAGS_perc_stock_level;

  /**
   * 0                                                    UINT64_MAX
   * |----|--------|--------|--------------|--------------|
   *      ^        ^        ^              ^
   *      |        |        |              threshold_new_order
   *      |        |        threshold_payment
   *      |        threshold_order_status
   *      threshold_delivery
   *
   * used by decideQueryType().
   */
  std::uint64_t threshold_new_order;
  std::uint64_t threshold_payment;
  std::uint64_t threshold_order_status;
  std::uint64_t threshold_delivery;

  Option() {
    threshold_delivery = perc_stock_level * (UINT64_MAX / 100);
    threshold_order_status = threshold_delivery + (perc_delivery * (UINT64_MAX / 100));
    threshold_payment = threshold_order_status + (perc_order_status * (UINT64_MAX / 100));
    threshold_new_order = threshold_payment + (perc_payment * (UINT64_MAX / 100));
#if 0
    ::printf("query_type_threshold: %.3f %.3f %.3f %.3f\n"
             , threshold_new_order    / (double)UINT64_MAX
             , threshold_payment      / (double)UINT64_MAX
             , threshold_order_status / (double)UINT64_MAX
             , threshold_delivery     / (double)UINT64_MAX);
#endif
  }
};

class NewOrder {
public:
  std::uint16_t w_id;
  std::uint8_t d_id;
  std::uint32_t c_id;
  struct {
    std::uint32_t ol_i_id;
    std::uint16_t ol_supply_w_id;
    std::uint8_t ol_quantity;
  } items[15];
  std::uint8_t rbk;
  bool remote;
  std::uint8_t ol_cnt;

  void generate(uint16_t w_id0, Option &opt);

  void print();
};

class Payment {
public:
  std::uint16_t w_id;
  std::uint8_t d_id;
  std::uint32_t c_id;
  std::uint16_t d_w_id;
  std::uint16_t c_w_id;
  std::uint8_t c_d_id;
  char c_last[LASTNAME_LEN + 1];
  double h_amount;
  bool by_last_name;

  void generate(uint16_t w_id0, Option &opt);

  void print();
};

class OrderStatus {
};

class Delivery {
};

class StockLevel {
};

} // namespace query

class Query {
public:
  QueryType type = Q_NONE;
  union {
    query::NewOrder new_order;
    query::Payment payment;
    query::OrderStatus order_status;
    query::Delivery delivery;
    query::StockLevel stock_level;
  };

  void generate(uint16_t w_id0, query::Option &opt);

  void print();
};
}
