/**
 * @file tpcc_query.cpp
 */

#include <cstring>
#include "../include/random.hh"
#include "tpcc_query.hpp"
#include "tpcc_util.hpp"
#include "epoch.h"


#define ID_START 1

#if 1
#define FIXED_WAREHOUSE_PER_THREAD
// In TPC-C specification 5.11, there exists the following explanation:
// "2.5.1.1 For any given terminal, the home warehouse number (W_ID) is constant over the whole measurement interval.",
// so each thread can use just one w_id.
#else
#undef FIXED_WAREHOUSE_PER_THREAD
#endif


namespace TPCC {


void query::NewOrder::generate([[maybe_unused]]uint16_t w_id0, query::Option &opt) {

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

void query::Payment::generate([[maybe_unused]]std::uint16_t w_id0, query::Option &opt) {

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

void query::NewOrder::print() {
  printf("nod: w_id=%" PRIu16 " d_id=%" PRIu8 " c_id=%" PRIu32 " rbk=%" PRIu8 " remote=%s ol_cnt=%" PRIu8 "\n",
         w_id, d_id, c_id, rbk, remote ? "t" : "f", ol_cnt);
  for (unsigned int i = 0; i < ol_cnt; ++i) {
    printf(" [%d]: ol_i_id=%" PRIu32 " ol_supply_w_id=%" PRIu16 " c_quantity=%" PRIu8 "\n", i,
           items[i].ol_i_id, items[i].ol_supply_w_id, items[i].ol_quantity);
  }
}

void query::Payment::print() {
  printf("pay: w_id=%" PRIu16 " d_id=%" PRIu8 " d_w_id=%" PRIu16 " c_w_id=%" PRIu16 " c_d_id=%" PRIu8 " h_amount=%.2f\n",
         w_id, d_id, d_w_id, c_w_id, c_d_id, h_amount);
  if (by_last_name) {
    printf(" by_last_name=t c_last=%s\n", c_last);
  } else {
    printf(" by_last_name=f c_id=%" PRIu32 "\n", c_id);
  }

}

static QueryType decideQueryType(query::Option &opt) {
  uint64_t x = random_64bits();
  if (x >= opt.threshold_new_order) return Q_NEW_ORDER;
  if (x >= opt.threshold_payment) return Q_PAYMENT;
  if (x >= opt.threshold_order_status) return Q_ORDER_STATUS;
  if (x >= opt.threshold_delivery) return Q_DELIVERY;
  return Q_STOCK_LEVEL;
}

void Query::generate(std::uint16_t w_id, query::Option &opt) {
  type = decideQueryType(opt);
  switch (type) {
    case Q_NEW_ORDER:
      new_order.generate(w_id, opt);
      break;
    case Q_PAYMENT:
      payment.generate(w_id, opt);
      break;
    case Q_ORDER_STATUS:
    case Q_DELIVERY:
    case Q_STOCK_LEVEL:
    case Q_NONE:
      std::abort();
  }
}

void Query::print() {
  switch (type) {
    case Q_NEW_ORDER :
      new_order.print();
      break;
    case Q_PAYMENT :
      payment.print();
      break;
    case Q_ORDER_STATUS:
    case Q_DELIVERY:
    case Q_STOCK_LEVEL:
    case Q_NONE:
      std::abort();
  }
}
}


#ifdef TEST
#define N 2000000

int main(int argc, char *argv[]) {
  Result res;
  Xoroshiro128Plus rnd;
  //rnd.init();
  TPCC::query::Option query_opt;
  TPCC::Query *query = (TPCC::Query*)malloc(sizeof(TPCC::Query)*N);

  //query_opt.perc_payment = 100;
  for (int i=0; i<N; ++i) {
    query[i].generate(rnd,query_opt,res);
  }

  for (int i=0; i<5; ++i) {
    query[i].print();
  }
  puts("");
  for (int i=N-5; i<N; ++i) {
    query[i].print();
  }
  free(query);
  return 0;
}
#endif
