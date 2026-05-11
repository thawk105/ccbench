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
class DeterministicSbombWorkload : public BombWorkload<Tuple,Param> {
public:
    std::map<uint32_t,Node*> bom_cache_;

    DeterministicSbombWorkload() {}
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

      // std::cout << "prepare " << tx.thid_ << std::endl;
      // for (const auto& [p_id, root] : bom_cache_) {
      //   std::vector<Node*> nodes;
      //   root->get_all_nodes(nodes);
      //   for (auto& node : nodes) {
      //     if (node->is_leaf()) {
      //       if (tx.thid_ == 0) std::cout << p_id << " : " << node->i_id_ << std::endl;
      //     }
      //   }
      // }
    }

    void run_issue_journal_voucher(TxExecutor& tx, Query& query) {
      uint32_t i = 0;
      uint32_t f_id = query.args.f_id;
      SimpleKey<8> keys[bom_cache_.size()];

      // prepare keys
      for (const auto& [p_id, root] : bom_cache_) {
        ProductCostMaster::CreateKey(f_id, p_id, keys[i].ptr());
        tx.lock_entries_.emplace_back(Storage::ProductCostMaster, keys[i].view(), false);
        i++;
      }

      // sort keys and lock
      tx.lockList();

      // access record
      SimpleKey<8> low, up;
      std::vector<TupleBody*> result;
      ProductCostMaster::CreateKey(f_id, 0, low.ptr());
      ProductCostMaster::CreateKey(f_id+1, 0, up.ptr());
      tx.scan(Storage::ProductCostMaster, low.view(), false, up.view(), true, result);
      if (FLAGS_bomb_interactive_ms) sleepMs(FLAGS_bomb_interactive_ms);

      uint32_t debit = 0;  // product
      uint32_t credit = 1; // work in progress
      for (auto& tuple : result) {
        const ProductCostMaster& pm = tuple->get_value().cast_to<ProductCostMaster>();
        double manufactured_quantity = 99.9;
        BombWorkload<Tuple,Param>::insert_journal_voucher(tx, debit, credit, manufactured_quantity*pm.pc_cost);
      }
      if (FLAGS_bomb_interactive_ms) sleepMs(FLAGS_bomb_interactive_ms); // as batch
    }
  
    void run_update_material_cost_master(TxExecutor& tx, Query& query) {
      uint32_t i = 0;
      SimpleKey<8> keys[query.args.i_id_set.size()];

      // prepare keys
      for (auto& m_id : query.args.i_id_set) {
        SimpleKey<8> key;
        MaterialCostMaster::CreateKey(query.args.f_id, m_id, keys[i].ptr());
        tx.lock_entries_.emplace_back(Storage::MaterialCostMaster, keys[i].view(), true);
        i++;
      }

      // sort keys and lock
      tx.lockList();

      // access record
      for (auto& m_id : query.args.i_id_set) {
        SimpleKey<8> key;
        MaterialCostMaster::CreateKey(query.args.f_id, m_id, key.ptr());
        TupleBody* body;
        tx.read(Storage::MaterialCostMaster, key.view(), &body);
        MaterialCostMaster& old = body->get_value().cast_to<MaterialCostMaster>();
        HeapObject obj;
        obj.template allocate<MaterialCostMaster>();
        MaterialCostMaster& mc = obj.ref();
        mc.mc_f_id = old.mc_f_id;
        mc.mc_i_id = old.mc_i_id;
        mc.mc_stock_quantity = old.mc_stock_quantity + 1.0;
        mc.mc_stock_price = old.mc_stock_price + 1.0;
        tx.write(Storage::MaterialCostMaster, key.view(), TupleBody(key.view(), std::move(obj)));
      }
      if (FLAGS_bomb_interactive_ms) sleepMs(FLAGS_bomb_interactive_ms); // as batch
    }

    void run_update_prodcut_cost_master(TxExecutor& tx, Query& query) {
      uint32_t i = 0;
      uint32_t f_id = query.args.f_id;
      SimpleKey<8>* keys;
      if (posix_memalign((void **) &keys, CACHE_LINE_SIZE,
              FLAGS_bomb_base_product_size_per_factory
              + FLAGS_bomb_base_product_size_per_factory
              * FLAGS_bomb_tree_num_per_product
              * FLAGS_bomb_base_tree_size
              * FLAGS_bomb_material_per_wip * sizeof(SimpleKey<8>)) != 0) ERR;

      // prepare keys
      for (const auto& [p_id, root] : bom_cache_) {
        //std::cout << p_id << " : " << root << std::endl;
        std::vector<Node*> nodes;
        root->get_all_nodes(nodes);
        for (auto& node : nodes) {
          if (node->is_leaf()) {
            MaterialCostMaster::CreateKey(f_id, node->i_id_, keys[i].ptr());
            tx.lock_entries_.emplace_back(Storage::MaterialCostMaster, keys[i].view(), false);
          }
          i++;
        }
        ProductCostMaster::CreateKey(f_id, p_id, keys[i].ptr());
        tx.lock_entries_.emplace_back(Storage::ProductCostMaster, keys[i].view(), true);
        i++;
      }

      // sort keys and lock
      tx.lockList();

      // access record
      for (const auto& [p_id, root] : bom_cache_) {
        std::vector<Node*> nodes;
        root->get_all_nodes(nodes);
        for (auto& node : nodes) {
          if (node->is_leaf()) {
            if (!BombWorkload<Tuple,Param>::get_material_cost(tx, f_id, node->i_id_, node->unit_cost_))
              return;
          }
        }
        BombWorkload<Tuple,Param>::update_product_cost_master(tx, f_id, p_id, root->calculate_cost());
        if (tx.quit_) { // for long read phase
          tx.status_ = TransactionStatus::invalid;
          return;
        }
      }
      free(keys);
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
            run_issue_journal_voucher(tx, query);
            break;
          case TxType::UpdateMaterialCostMaster:
            run_update_material_cost_master(tx, query);
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
