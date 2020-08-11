#include "../include/random.hh"
#include "../include/result.hh"

#pragma once

#define TPCC_SMALL	false
#define NUM_WH          224
#define DIST_PER_WARE	10
#define PERC_PAYMENT 	0.5
#define LASTNAME_LEN 	16

namespace TPCC {
    std::uint32_t g_num_wh = NUM_WH;
    std::uint32_t g_dist_per_ware = DIST_PER_WARE;
    double        g_perc_payment = PERC_PAYMENT;
#if TPCC_SMALL
    std::uint32_t g_max_items = 10000;
    std::uint32_t g_cust_per_dist = 2000;
#else
    std::uint32_t g_max_items = 100000;
    std::uint32_t g_cust_per_dist = 3000;
#endif

    enum QueryType { Q_NONE,
                     Q_NEW_ORDER,
                     Q_PAYMENT,
                     Q_ORDER_STATUS,
                     Q_DELIVERY,
                     Q_STOCK_LEVEL };

    namespace query {

        class NewOrder {
        public:
            std::uint64_t w_id;
            std::uint64_t d_id;
            std::uint64_t c_id;
            struct {
                std::uint64_t ol_i_id;
                std::uint64_t ol_supply_w_id;
                std::uint64_t ol_quantity;
            } items[15];
            bool rbk;
            bool remote;
            std::uint64_t ol_cnt;
            std::uint64_t o_entry_d;

            void generate(Xoroshiro128Plus &rnd,
                          [[maybe_unused]]Result &res);
            void print();
        };

        class Payment {
        public:
            std::uint64_t w_id;
            std::uint64_t d_id;
            std::uint64_t c_id;
            std::uint64_t d_w_id;
            std::uint64_t c_w_id;
            std::uint64_t c_d_id;
            char c_last[LASTNAME_LEN];
            double h_amount;
            bool by_last_name;

            void generate(Xoroshiro128Plus &rnd,
                          [[maybe_unused]]Result &res);
            void print();
        };

        class OrderStatus {};
        class Delivery {};
        class StockLevel {};
    }

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

        void generate(Xoroshiro128Plus &rnd,
                      [[maybe_unused]]Result &res);
        void print();
    };
}
