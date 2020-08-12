#include <cstring>
#include "./tpcc_query.hpp"
#include "../include/random.hh"
#include "../include/result.hh"
#include "tpcc_tables.hpp"
#include "./include/record.h"
#include "./include/interface.h"
#include "./index/masstree_beta/include/masstree_beta_wrapper.h"


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
        return;
    }

    std::uint64_t Random(std::uint64_t x, std::uint64_t y, Xoroshiro128Plus &rnd) {
        if (x==y) return x;
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
            abort();
        }
        return (((Random(0,A,rnd) | Random(x,y,rnd)) + C) % (y-x+1)) + x;
    }

    void query::NewOrder::generate(Xoroshiro128Plus &rnd,
                                   [[maybe_unused]]Result &res) {
        w_id   = Random(1, g_num_wh, rnd);
        d_id   = Random(1, g_dist_per_ware, rnd);
        c_id   = NURand(1023, 1, g_cust_per_dist, rnd);
        rbk    = Random(1, 100, rnd);
        ol_cnt = Random(5, 15, rnd);
        o_entry_d = 2013;
        remote = false;

        for (unsigned int i=0; i<ol_cnt; ++i) {
            { redo1:
                items[i].ol_i_id = NURand(8191, 1, g_max_items, rnd);
                for (unsigned int j=0; j<i; ++j) {
                    if (items[i].ol_i_id == items[j].ol_i_id) goto redo1;
                }
            };
            int x = Random(1, 100, rnd);
            if (x > 1 || g_num_wh == 1) {
                items[i].ol_supply_w_id = w_id;
            } else {
                do {
                    items[i].ol_supply_w_id = Random(1, g_num_wh, rnd);
                } while (items[i].ol_supply_w_id == w_id);
                remote = true;
            }
            items[i].ol_quantity = Random(1, 10, rnd);
        }
    }

    void query::Payment::generate(Xoroshiro128Plus &rnd,
                                  [[maybe_unused]]Result &res) {
        w_id = Random(1, g_num_wh, rnd);
        d_w_id = w_id;
        d_id = Random(1, g_dist_per_ware, rnd);
        h_amount = Random(100, 500000, rnd)*0.01;
        int x = Random(1, 100, rnd);
        int y = Random(1, 100, rnd);

        if (x <= 85) {
            // home warehouse
            c_d_id = d_id;
            c_w_id = w_id;
        } else {
            // remote warehouse
            c_d_id = Random(1, g_dist_per_ware, rnd);
            if (g_num_wh > 1) {
                do {
                    c_w_id = Random(1, g_num_wh, rnd);
                } while (c_w_id == w_id);
            } else {
                c_w_id = w_id;
            }
        }
        if (y <= 60) {
            // by last name
            by_last_name = true;
            Lastname(NURand(255, 0, 999, rnd), c_last);
        } else {
            // by cust id
            by_last_name = false;
            c_id = NURand(1023, 1, g_cust_per_dist, rnd);
        }
    }

    void query::NewOrder::print() {
        printf("nod: w_id=%lu d_id=%lu c_id=%lu rbk=%s remote=%s ol_cnt=%lu o_entry_d=%lu\n",
               w_id, d_id, c_id, rbk?"t":"f", remote?"t":"f", ol_cnt, o_entry_d);
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

    void Query::generate(Xoroshiro128Plus &rnd,
                         [[maybe_unused]]Result &res) {
        double x = rnd.next() / (((double)~(uint64_t)0)+1.0);

        /*
        if (x < g_perc_payment) {
            type = Q_PAYMENT;
        } else {
            type = Q_NEW_ORDER;
        }
        */
       
       type=Q_PAYMENT;


        switch (type) {
        case Q_NEW_ORDER:
            new_order.generate(rnd,res);
            break;
        case Q_PAYMENT:
            payment.generate(rnd,res);
            break;
        case Q_ORDER_STATUS:
        case Q_DELIVERY:
        case Q_STOCK_LEVEL:
        case Q_NONE:
            abort();
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
            abort();
        }
    }


    void run_payment(TPCC::query::Payment &query,Xoroshiro128Plus &rnd){
        uint64_t W_ID=query.w_id;
        uint64_t D_ID=query.d_id;

        uint64_t C_W_ID=query.c_w_id;
        uint64_t C_D_ID=query.c_d_id;
        size_t x = Random(1, 100, rnd);
        size_t y = Random(1,100,rnd);

        std::string customer_key;
        customer_key=TPCC::Customer::CreateKey(C_W_ID, C_D_ID, D_ID);
        
        const double H_AMOUNT=query.h_amount;
        std::time_t H_DATE=std::time(nullptr);

        Warehouse warehouse;
        {
            //ここでkey=CreateKey(W_ID)をよみたい
            std::string key=Warehouse::CreateKey(W_ID);
            //Record* record=kohler_masstree::find(record(Storage::WAREHOUSE,key));
        }
    }
}


#define TEST true

#if TEST
#define N 2000000

int main(void) {
    Result res;
    Xoroshiro128Plus rnd;
    // TPCC::Query *query = (TPCC::Query*)malloc(sizeof(TPCC::Query)*N);
    // for (int i=0; i<N; ++i) {
    //     query[i].generate(rnd,res);
    // }
    // for (int i=0; i<5; ++i) {
    //     query[i].print();
    // }
    //free(query);

    TPCC::Query query;
    query.generate(rnd,res);
    switch(query.type){
        case TPCC::Q_PAYMENT:
            TPCC::run_payment(query.payment,rnd);
            break;

    }


    
    return 0;
}
#endif
