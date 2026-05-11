#pragma once

#include "tpcc_common.hh"
#include "tpcc_util.hh"
// #include "../include/random.hh"
// #include "../include/result.hh"

#define ID_START 1

#if 1
#define FIXED_WAREHOUSE_PER_THREAD
// In TPC-C specification 5.11, there exists the following explanation:
// "2.5.1.1 For any given terminal, the home warehouse number (W_ID) is constant over the whole measurement interval.",
// so each thread can use just one w_id.
#else
#undef FIXED_WAREHOUSE_PER_THREAD
#endif

enum class TxType : std::uint32_t {
  None = 0,
  NewOrder,
  Payment,
  OrderStatus,
  Delivery,
  StockLevel,
};

namespace TPCCQuery {

class Option {
public:
  std::uint32_t num_wh = FLAGS_tpcc_num_wh;
  std::uint32_t dist_per_ware = DIST_PER_WARE;
  std::uint32_t max_items = MAX_ITEMS;
  std::uint32_t cust_per_dist = CUST_PER_DIST;
  std::uint64_t perc_payment = FLAGS_tpcc_perc_payment;
  std::uint64_t perc_order_status = FLAGS_tpcc_perc_order_status;
  std::uint64_t perc_delivery = FLAGS_tpcc_perc_delivery;
  std::uint64_t perc_stock_level = FLAGS_tpcc_perc_stock_level;

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

  void generate([[maybe_unused]]uint16_t w_id0, Option &opt) {
#ifdef FIXED_WAREHOUSE_PER_THREAD
    w_id = w_id0;
#else
    w_id = random_int(ID_START, opt.num_wh);
#endif
    d_id = random_int(ID_START, opt.dist_per_ware);
    c_id = non_uniform_random<1023>(ID_START, opt.cust_per_dist);
    rbk = random_int(1, 100);
    ol_cnt = random_int(5, 15);

    for (unsigned int i = 0; i < ol_cnt; ++i) {
#if 0 // ol_i_id is no need to be unique.
      { redo1:
        items[i].ol_i_id = non_uniform_random<8191>(ID_START, opt.max_items);
        for (unsigned int j=0; j<i; ++j) {
          if (items[i].ol_i_id == items[j].ol_i_id) goto redo1;
        }
      }
#else
      items[i].ol_i_id = non_uniform_random<8191>(ID_START, opt.max_items);
#endif
      if (opt.num_wh == 1 || random_int(0, 99) != 0) {
        items[i].ol_supply_w_id = w_id;
        remote = false;
      } else {
        do {
          items[i].ol_supply_w_id = random_int(ID_START, opt.num_wh);
        } while (items[i].ol_supply_w_id == w_id);
        remote = true;
      }
      items[i].ol_quantity = random_int(1, 10);
    }
    if (rbk == 1) { // set an unused item number to produce "not-found" for roll back
      items[ol_cnt - 1].ol_i_id += opt.max_items;
    }
  }

  void print() {
    printf("nod: w_id=%" PRIu16 " d_id=%" PRIu8 " c_id=%" PRIu32 " rbk=%" PRIu8 " remote=%s ol_cnt=%" PRIu8 "\n",
          w_id, d_id, c_id, rbk, remote ? "t" : "f", ol_cnt);
    for (unsigned int i = 0; i < ol_cnt; ++i) {
      printf(" [%d]: ol_i_id=%" PRIu32 " ol_supply_w_id=%" PRIu16 " c_quantity=%" PRIu8 "\n", i,
            items[i].ol_i_id, items[i].ol_supply_w_id, items[i].ol_quantity);
    }
  }
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

  void generate([[maybe_unused]]std::uint16_t w_id0, Option &opt) {
#ifdef FIXED_WAREHOUSE_PER_THREAD
    w_id = w_id0;
#else
    w_id = random_int(ID_START, opt.num_wh);
#endif
    d_w_id = w_id;
    d_id = random_int(ID_START, opt.dist_per_ware);
    h_amount = random_double(100, 500000, 100);

    size_t x = random_int(1, 100);
    if (x <= 85) {
      // home warehouse
      c_d_id = d_id;
      c_w_id = w_id;
    } else {
      // remote warehouse
      c_d_id = random_int(ID_START, opt.dist_per_ware);
      if (opt.num_wh > 1) {
        do {
          c_w_id = random_int(ID_START, opt.num_wh);
        } while (c_w_id == w_id);
      } else {
        c_w_id = w_id;
      }
    }

    size_t y = random_int(1, 100);
    if (y <= 60) {
      // by last name
      by_last_name = true;
      make_c_last(non_uniform_random<255>(0, 999), c_last);
    } else {
      // by cust id
      by_last_name = false;
      c_id = non_uniform_random<1023>(ID_START, opt.cust_per_dist);
    }
  }

  void print() {
    printf("pay: w_id=%" PRIu16 " d_id=%" PRIu8 " d_w_id=%" PRIu16 " c_w_id=%" PRIu16 " c_d_id=%" PRIu8 " h_amount=%.2f\n",
          w_id, d_id, d_w_id, c_w_id, c_d_id, h_amount);
    if (by_last_name) {
      printf(" by_last_name=t c_last=%s\n", c_last);
    } else {
      printf(" by_last_name=f c_id=%" PRIu32 "\n", c_id);
    }
  }
};

class OrderStatus {
public:
  std::uint16_t w_id;
  std::uint8_t d_id;
  std::uint32_t c_id;
  char c_last[LASTNAME_LEN + 1];
  bool by_last_name;

  void generate(uint16_t w_id0, Option &opt) {
#ifdef FIXED_WAREHOUSE_PER_THREAD
    w_id = w_id0;
#else
    w_id = random_int(ID_START, opt.num_wh);
#endif
    d_id = random_int(ID_START, opt.dist_per_ware);

    size_t y = random_int(1, 100);
    if (y <= 60) {
      // by last name
      by_last_name = true;
      make_c_last(non_uniform_random<255>(0, 999), c_last);
    } else {
      // by cust id
      by_last_name = false;
      c_id = non_uniform_random<1023>(ID_START, opt.cust_per_dist);
    }
  }

  void print() {
    printf("ost: w_id=%" PRIu16 " d_id=%" PRIu8 "\n", w_id, d_id);
    if (by_last_name) {
      printf(" by_last_name=t c_last=%s\n", c_last);
    } else {
      printf(" by_last_name=f c_id=%" PRIu32 "\n", c_id);
    }
  }
};

class Delivery {
public:
  std::uint16_t w_id;
  std::uint8_t o_carrier_id;
  std::uint64_t ol_delivery_d;

  void generate(uint16_t w_id0, Option &opt) {
#ifdef FIXED_WAREHOUSE_PER_THREAD
    w_id = w_id0;
#else
    w_id = random_int(ID_START, opt.num_wh);
#endif
    o_carrier_id = random_int(1, 10);
    ol_delivery_d = get_lightweight_timestamp();
  }

  void print() {
    printf("del: w_id=%" PRIu16 " o_carrier_id=%" PRIu8 "ol_delivery_d=%" PRIu64 "\n",
          w_id, o_carrier_id, ol_delivery_d);
  }
};

class StockLevel {
public:
  std::uint16_t w_id;
  std::uint8_t d_id;
  std::uint8_t threshold;

  void generate(uint16_t w_id0, Option &opt) {
#ifdef FIXED_WAREHOUSE_PER_THREAD
    w_id = w_id0;
#else
    w_id = random_int(ID_START, opt.num_wh);
#endif
    d_id = random_int(1, opt.dist_per_ware);
    threshold = random_int(10, 20);
  }

  void print() {
    printf("stklvl: w_id=%" PRIu16 " d_id=%" PRIu8 " threshold=%" PRIu8, w_id, d_id, threshold);
  }
};

} // namespace TPCCQuery

static TxType decideQueryType(TPCCQuery::Option &opt) {
  uint64_t x = random_64bits();
  if (x >= opt.threshold_new_order) return TxType::NewOrder;
  if (x >= opt.threshold_payment) return TxType::Payment;
  if (x >= opt.threshold_order_status) return TxType::OrderStatus;
  if (x >= opt.threshold_delivery) return TxType::Delivery;
  return TxType::StockLevel;
}

class Query {
public:
  TxType type = TxType::None;
  union {
    TPCCQuery::NewOrder new_order;
    TPCCQuery::Payment payment;
    TPCCQuery::OrderStatus order_status;
    TPCCQuery::Delivery delivery;
    TPCCQuery::StockLevel stock_level;
  };

  void generate(std::uint16_t w_id, TPCCQuery::Option &opt) {
    type = decideQueryType(opt);
    switch (type) {
      case TxType::NewOrder:
        new_order.generate(w_id, opt);
        break;
      case TxType::Payment:
        payment.generate(w_id, opt);
        break;
      case TxType::OrderStatus:
        order_status.generate(w_id, opt);
        break;
      case TxType::Delivery:
        delivery.generate(w_id, opt);
        break;
      case TxType::StockLevel:
        stock_level.generate(w_id, opt);
        break;
      case TxType::None:
        std::abort();
    }
  }

  void print() {
    switch (type) {
      case TxType::NewOrder :
        new_order.print();
        break;
      case TxType::Payment :
        payment.print();
        break;
      case TxType::OrderStatus:
        order_status.print();
        break;
      case TxType::Delivery:
        delivery.print();
        break;
      case TxType::StockLevel:
        stock_level.print();
        break;
      case TxType::None:
        std::abort();
    }
  }
};
