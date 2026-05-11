#pragma once

#include <atomic>
#include <thread>
#include <vector>
#include <set>

#include "bomb.hh"
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

#include "gflags/gflags.h"

template <typename Tuple, typename Param>
class StaticBombWorkload : public BombWorkload<Tuple,Param> {
public:
    std::map<uint32_t,Node*> bom_cache_;

    StaticBombWorkload() {}
    using Query = typename BombWorkload<Tuple,Param>::Query;

    void prepare(TxExecutor& tx, Param *p) {
      BombWorkload<Tuple,Param>::prepare(tx, p);
 
      // for static BoM
      tx.begin();
      std::vector<uint32_t> product_ids;
      auto ret = BombWorkload<Tuple,Param>::select_im_by_factory(tx, 1, product_ids);
      for (auto& p_id : product_ids) {
        Node* root = BombWorkload<Tuple,Param>::build_bom_tree(tx, p_id);
        if (root == nullptr) ERR;
        bom_cache_.emplace(p_id, root);
      }
      tx.commit();
    }

    void run_update_prodcut_cost_master(TxExecutor& tx, Query& query) {
      uint32_t f_id = query.args.f_id;
      for (const auto& [p_id, root] : bom_cache_) {
        std::vector<Node*> nodes;
        root->get_all_nodes(nodes);
        for (auto& node : nodes) {
          if (node->is_leaf()) {
            // std::cout << p_id << " : " << node->i_id_ << std::endl;
            if (!BombWorkload<Tuple,Param>::get_material_cost(tx, f_id, node->i_id_, node->unit_cost_))
              return;
          }
        }
        BombWorkload<Tuple,Param>::update_product_cost_master(tx, f_id, p_id, root->calculate_cost());
        if (tx.status_ == TransactionStatus::aborted) return;
        if (tx.quit_) { // for long read phase
          tx.status_ = TransactionStatus::invalid;
          return;
        }
      }
    }

    template <typename TxExecutor, typename TransactionStatus>
    void run(TxExecutor& tx) {
        Query query;
        query.generate(this, tx);

RETRY:
        if (tx.isLeader()) {
            tx.leaderWork();
        }

        if (loadAcquire(tx.quit_)) return;

        tx.begin();

        switch (query.type) {
          case TxType::IssueJournalVoucher:
            BombWorkload<Tuple,Param>::run_issue_journal_voucher(tx, query);
            break;
          case TxType::UpdateMaterialCostMaster:
            BombWorkload<Tuple,Param>::run_update_material_cost_master(tx, query);
            break;
          case TxType::UpdateProductCostMaster:
            run_update_prodcut_cost_master(tx, query);
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
};
