#include <cstring>
#include "../include/random.hh"
#include "../include/result.hh"
#include "tpcc_query.hpp"

#define ID_START 1

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

void query::NewOrder::generate(Xoroshiro128Plus &rnd, query::Option &opt,
                               [[maybe_unused]]Result &res) {
  w_id   = Random(ID_START, opt.num_wh, rnd);
  d_id   = Random(ID_START, opt.dist_per_ware, rnd);
  c_id   = NURand(1023, ID_START, opt.cust_per_dist, rnd);
  rbk    = Random(1, 100, rnd);
  ol_cnt = Random(5, 15, rnd);
  o_entry_d = 2013;
  remote = false;

  for (unsigned int i=0; i<ol_cnt; ++i) {
    { redo1:
      items[i].ol_i_id = NURand(8191, ID_START, opt.max_items, rnd);
      for (unsigned int j=0; j<i; ++j) {
        if (items[i].ol_i_id == items[j].ol_i_id) goto redo1;
      }
    }
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

void query::Payment::generate(Xoroshiro128Plus &rnd, query::Option &opt,
                              [[maybe_unused]]Result &res) {
  w_id = Random(ID_START, opt.num_wh, rnd);
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
  if (y < 1) { //if (y <= 60) { // disable by_last_name for test
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
  printf("nod: w_id=%lu d_id=%lu c_id=%lu rbk=%u remote=%s ol_cnt=%lu o_entry_d=%lu\n",
         w_id, d_id, c_id, rbk, remote?"t":"f", ol_cnt, o_entry_d);
  for (unsigned int i=0; i<ol_cnt; ++i) {
    printf(" [%d]: ol_i_id=%lu ol_supply_w_id=%lu c_quantity=%lu\n", i,
           items[i].ol_i_id, items[i].ol_supply_w_id, items[i].ol_quantity);
  }
}

void query::Payment::print() {
  printf("pay: w_id=%lu d_id=%lu d_w_id=%lu c_w_id=%lu c_d_id=%lu h_amount=%.2f\n",
         w_id, d_id, d_w_id, c_w_id, c_d_id, h_amount);
  if (by_last_name) {
    printf(" by_last_name=t c_last=%s\n",c_last);
  } else {
    printf(" by_last_name=f c_id=%lu\n",c_id);
  }

}

void Query::generate(Xoroshiro128Plus &rnd, query::Option &opt,
                     [[maybe_unused]]Result &res) {
  double x = rnd.next() / (((double)~(uint64_t)0)+1.0) * 100;
  x -= opt.perc_stock_level;
  if (x < 0) {
    type = Q_STOCK_LEVEL;
  } else {
    x -= opt.perc_delivery;
    if (x < 0) {
      type = Q_DELIVERY;
    } else {
      x -= opt.perc_order_status;
      if (x < 0) {
        type = Q_ORDER_STATUS;
      } else {
        x -= opt.perc_payment;
        if (x < 0) {
          type = Q_PAYMENT;
        } else {
          type = Q_NEW_ORDER;
        }
      }
    }
  }
  switch (type) {
  case Q_NEW_ORDER:
    new_order.generate(rnd,opt,res);
    break;
  case Q_PAYMENT:
    payment.generate(rnd,opt,res);
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
