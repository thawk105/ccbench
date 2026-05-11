#pragma once

#include <atomic>
#include <thread>
#include <vector>
#include <set>

#include "config.hh"
#include "debug.hh"
#include "heap_object.hh"
#include "procedure.hh"
#include "random.hh"
#include "result.hh"
#include "tuple_body.hh"
#include "util.hh"
#include "workload.hh"
#include "zipf.hh"

#include "./tpcc/tpcc_initializer.hh"
#include "./tpcc/tpcc_query.hh"
#include "./tpcc/tpcc_tables.hh"
#include "./tpcc/tpcc_tx_neworder.hh"
#include "./tpcc/tpcc_tx_payment.hh"
#include "./tpcc/tpcc_tx_orderstatus.hh"
#include "./tpcc/tpcc_tx_delivery.hh"
#include "./tpcc/tpcc_tx_stocklevel.hh"

template <typename Tuple, typename Param>
class TPCCWorkload {
public:
    Param* param_;
    Xoroshiro128Plus rnd_;
    HistoryKeyGenerator hkg_;
    uint16_t w_id; // home warehouse

    TPCCWorkload() {
      rnd_.init();
    }

    void prepare(TxExecutor& tx, Param *p) {
      hkg_.init(tx.thid_, true);
      w_id = (tx.thid_ % FLAGS_tpcc_num_wh) + 1; // home warehouse.
    }

    template <typename TxExecutor, typename TransactionStatus>
    void run(TxExecutor& tx) {
        Query query;
        TPCCQuery::Option option;

RETRY:
        query.generate(w_id, option);

        if (tx.isLeader()) {
            tx.leaderWork();
        }

        if (loadAcquire(tx.quit_)) return;

        tx.begin();

        switch (query.type) {
          case TxType::NewOrder:
            if (!run_new_order<TxExecutor,TransactionStatus>(tx, &query.new_order)) {
              tx.status_ = TransactionStatus::aborted;
            }
            break;
          case TxType::Payment:
            if (!run_payment<TxExecutor,TransactionStatus,Tuple>(tx, &query.payment, &hkg_)) {
              tx.status_ = TransactionStatus::aborted;
            }
            break;
          case TxType::OrderStatus:
            if (!run_order_status<TxExecutor,TransactionStatus,Tuple>(tx, &query.order_status)) {
              tx.status_ = TransactionStatus::aborted;
            }
            break;
          case TxType::Delivery:
            if (!run_delivery<TxExecutor,TransactionStatus>(tx, &query.delivery)) {
              tx.status_ = TransactionStatus::aborted;
            }
            break;
          case TxType::StockLevel:
            if (!run_stock_level<TxExecutor,TransactionStatus>(tx, &query.stock_level)) {
              tx.status_ = TransactionStatus::aborted;
            }
            break;
          default:
            ERR;
            break;
        }

        if (tx.status_ == TransactionStatus::aborted) {
            tx.abort();
            tx.result_->local_abort_counts_++;
            tx.result_->local_abort_counts_per_tx_[get_tx_type(query.type)]++;
#if ADD_ANALYSIS
            ++tx.result_->local_early_aborts_;
#endif
            goto RETRY;
        }

        if (!tx.commit()) {
            tx.abort();
            if (tx.status_ == TransactionStatus::invalid) return;
            tx.result_->local_abort_counts_++;
            tx.result_->local_abort_counts_per_tx_[get_tx_type(query.type)]++;
            goto RETRY;
        }

        if (loadAcquire(tx.quit_)) return;
        tx.result_->local_commit_counts_++;
        tx.result_->local_commit_counts_per_tx_[get_tx_type(query.type)]++;

        return;
    }

    static uint32_t getTableNum() {
      return (uint32_t)Storage::Size;
    }

    static void makeDB([[maybe_unused]] Param* param) {
      Xoroshiro128Plus rand;
      rand.init();

      // TODO: move this codes to appropriate place
      // set_tx_name(TxType::xxx, "xxx");

      TPCCInitializaer<Tuple,Param>::load(param);
    }

    static void displayWorkloadParameter() {
    }

    static void displayWorkloadResult() {
    }
};
