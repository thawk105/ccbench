#include <cstring>
#include "../include/random.hh"
#include "../include/result.hh"
#include "tpcc_query.hpp"

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

/*==================================================================+
  | ROUTINE NAME
  |   Lastname
  | DESCRIPTION
  |   TPC-C Lastname Function.
  | ARGUMENTS
  |   num - non-uniform random number
  |   name - last name string
  +==================================================================*/
void Lastname(std::uint64_t num, char *name) {
  static const char *n[] =
    {"BAR", "OUGHT", "ABLE", "PRI", "PRES",
     "ESE", "ANTI", "CALLY", "ATION", "EING"};
  strcpy(name,n[num/100]);
  strcat(name,n[(num/10)%10]);
  strcat(name,n[num%10]);
}

std::uint64_t Random(std::uint64_t x, std::uint64_t y, Xoroshiro128Plus &rnd) {
  if (x==y) return x;
  if (x>y) std::abort();
  return (rnd.next() % (y-x+1)) + x;
}

std::uint64_t NURand(std::uint64_t A, std::uint64_t x, std::uint64_t y, Xoroshiro128Plus &rnd) {
  static int C_C_LAST = -1;
  static int C_C_ID = -1;
  static int C_OL_I_ID = -1;
  std::uint64_t C;

  switch(A) {
  case 255:
    if (C_C_LAST == -1) {
      C_C_LAST = Random(0, 255, rnd);
    }
    C = C_C_LAST;
    break;
  case 1023:
    if (C_C_ID == -1) {
      C_C_ID = Random(0, 1023, rnd);
    }
    C = C_C_ID;
    break;
  case 8191:
    if (C_OL_I_ID == -1) {
      C_OL_I_ID = Random(0, 8191, rnd);
    }
    C = C_OL_I_ID;
    break;
  default:
    std::abort();
  }
  return (((Random(0,A,rnd) | Random(x,y,rnd)) + C) % (y-x+1)) + x;
}

void query::NewOrder::generate([[maybe_unused]]uint16_t w_id0, Xoroshiro128Plus &rnd, query::Option &opt) {

#ifdef FIXED_WAREHOUSE_PER_THREAD
  w_id = w_id0;
#else
  w_id = Random(ID_START, opt.num_wh, rnd);
#endif
  d_id   = Random(ID_START, opt.dist_per_ware, rnd);
  c_id   = NURand(1023, ID_START, opt.cust_per_dist, rnd);
  rbk    = Random(1, 100, rnd);
  ol_cnt = Random(5, 15, rnd);
  o_entry_d = 2013;
  remote = false;

  for (unsigned int i=0; i<ol_cnt; ++i) {
#if 0 // ol_i_id is no need to be unique.
    { redo1:
      items[i].ol_i_id = NURand(8191, ID_START, opt.max_items, rnd);
      for (unsigned int j=0; j<i; ++j) {
        if (items[i].ol_i_id == items[j].ol_i_id) goto redo1;
      }
    }
#else
    items[i].ol_i_id = NURand(8191, ID_START, opt.max_items, rnd);
#endif
    if (opt.num_wh == 1 || Random(ID_START, 100, rnd) > 1) {
      items[i].ol_supply_w_id = w_id;
    } else {
      do {
        items[i].ol_supply_w_id = Random(ID_START, opt.num_wh, rnd);
      } while (items[i].ol_supply_w_id == w_id);
      remote = true;
    }
    items[i].ol_quantity = Random(1, 10, rnd);
  }
  if (rbk == 1) { // set an unused item number to produce "not-found" for roll back
    items[ol_cnt-1].ol_i_id += opt.max_items;
  }
}

void query::Payment::generate([[maybe_unused]]std::uint16_t w_id0, Xoroshiro128Plus &rnd, query::Option &opt) {

#ifdef FIXED_WAREHOUSE_PER_THREAD
  w_id = w_id0;
#else
  w_id = Random(ID_START, opt.num_wh, rnd);
#endif
  d_w_id = w_id;
  d_id = Random(ID_START, opt.dist_per_ware, rnd);
  h_amount = Random(100, 500000, rnd)*0.01;

  int x = Random(1, 100, rnd);
  if (x <= 85) {
    // home warehouse
    c_d_id = d_id;
    c_w_id = w_id;
  } else {
    // remote warehouse
    c_d_id = Random(ID_START, opt.dist_per_ware, rnd);
    if (opt.num_wh > 1) {
      do {
        c_w_id = Random(ID_START, opt.num_wh, rnd);
      } while (c_w_id == w_id);
    } else {
      c_w_id = w_id;
    }
  }

  int y = Random(1, 100, rnd);
#if 0 // by last name is still buggy... hoshino is debugging on it.
  if (y <= 60) {
#else
  if (y < 1) {
#endif
    // by last name
    by_last_name = true;
    Lastname(NURand(255, 0, 999, rnd), c_last);
  } else {
    // by cust id
    by_last_name = false;
    c_id = NURand(1023, ID_START, opt.cust_per_dist, rnd);
  }
}

void query::NewOrder::print() {
  printf("nod: w_id=%" PRIu16 " d_id=%" PRIu8 " c_id=%" PRIu32 " rbk=%" PRIu8 " remote=%s ol_cnt=%" PRIu8 " o_entry_d=%lu\n",
         w_id, d_id, c_id, rbk, remote?"t":"f", ol_cnt, (ulong)o_entry_d);
  for (unsigned int i=0; i<ol_cnt; ++i) {
    printf(" [%d]: ol_i_id=%" PRIu32 " ol_supply_w_id=%" PRIu16 " c_quantity=%" PRIu8 "\n", i,
           items[i].ol_i_id, items[i].ol_supply_w_id, items[i].ol_quantity);
  }
}

void query::Payment::print() {
  printf("pay: w_id=%" PRIu16 " d_id=%" PRIu8 " d_w_id=%" PRIu16 " c_w_id=%" PRIu16 " c_d_id=%" PRIu8 " h_amount=%.2f\n",
         w_id, d_id, d_w_id, c_w_id, c_d_id, h_amount);
  if (by_last_name) {
    printf(" by_last_name=t c_last=%s\n",c_last);
  } else {
    printf(" by_last_name=f c_id=%" PRIu32 "\n",c_id);
  }

}

static QueryType decideQueryType(Xoroshiro128Plus& rnd, query::Option& opt)
{
    uint64_t x = rnd.next();
    if (x >= opt.threshold_new_order) return Q_NEW_ORDER;
    if (x >= opt.threshold_payment) return Q_PAYMENT;
    if (x >= opt.threshold_order_status) return Q_ORDER_STATUS;
    if (x >= opt.threshold_delivery) return Q_DELIVERY;
    return Q_STOCK_LEVEL;
}

void Query::generate(std::uint16_t w_id, Xoroshiro128Plus &rnd, query::Option &opt)
{
  type = decideQueryType(rnd, opt);
  switch (type) {
  case Q_NEW_ORDER:
    new_order.generate(w_id,rnd,opt);
    break;
  case Q_PAYMENT:
    payment.generate(w_id,rnd,opt);
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
