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
class DeterministicBombWorkload : public BombWorkload<Tuple,Param> {
public:
    DeterministicBombWorkload() {}
    using Query = typename BombWorkload<Tuple,Param>::Query;

    void prepare(TxExecutor& tx, Param *p) {
      BombWorkload<Tuple,Param>::prepare(tx, p);
    }

    Status select_pc_by_factory(TxExecutor& tx, uint32_t factory_id, std::vector<uint32_t>& product_ids) {
      SimpleKey<8> low, up;
      std::vector<TupleBody*> result;
      ProductCostMaster::CreateKey(factory_id, 0, low.ptr());
      ProductCostMaster::CreateKey(factory_id+1, 0, up.ptr());
      Status stat = tx.scan(Storage::ProductCostMaster, low.view(), false, up.view(), true, result);
      if (tx.status_ == TransactionStatus::aborted) return stat;
      for (auto& tuple : result) {
        const ProductCostMaster& pc = tuple->get_value().cast_to<ProductCostMaster>();
        product_ids.emplace_back(pc.pc_product_id);
      }
      return stat;
    }

    void run_issue_journal_voucher(TxExecutor& tx, Query& query) {
RETRY:
      if (tx.quit_) return;
      uint32_t f_id = query.args.f_id;
      std::vector<uint32_t> product_ids;
      tx.reconnoiter_begin(); // Do target selection as if read from cache
      Status stat = select_pc_by_factory(tx, f_id, product_ids);
      tx.reconnoiter_end();

      // prepare keys
      uint32_t i = 0;
      SimpleKey<8> keys[product_ids.size()];
      for (const auto& p_id : product_ids) {
        ProductCostMaster::CreateKey(f_id, p_id, keys[i].ptr());
        tx.lock_entries_.emplace_back(Storage::ProductCostMaster, keys[i].view(), false);
        i++;
      }

      // sort keys and lock
      if (!tx.lockList()) {
        tx.unlockList();
        goto RETRY; // already reconnaissance fails due to deletion, so retry
      }

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
RETRY:
      if (tx.quit_) return;
      // prepare keys
      uint32_t i = 0;
      SimpleKey<8> keys[query.args.i_id_set.size()];
      for (auto& m_id : query.args.i_id_set) {
        SimpleKey<8> key;
        MaterialCostMaster::CreateKey(query.args.f_id, m_id, keys[i].ptr());
        tx.lock_entries_.emplace_back(Storage::MaterialCostMaster, keys[i].view(), true);
        i++;
      }

      // sort keys and lock
      if (!tx.lockList()) {
        tx.unlockList();
        goto RETRY; // already reconnaissance fails due to deletion, so retry
      }

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
      std::vector<uint32_t> product_ids;
      std::vector<uint32_t> verify_products;
      std::map<uint32_t,Node*> bom;
      std::map<uint32_t,Node*> verify_bom;

      tx.reconnoiter_begin(); // Do target selection as if read from cache
      auto ret = BombWorkload<Tuple,Param>::select_im_by_factory(tx, f_id, product_ids);
      for (auto& p_id : product_ids) {
        Node* root = BombWorkload<Tuple,Param>::build_bom_tree(tx, p_id);
        if (root == nullptr) ERR;
        bom.emplace(p_id, root);
      }
      tx.reconnoiter_end();

      // prepare keys
      SimpleKey<8>* keys;
      if (posix_memalign((void **) &keys, CACHE_LINE_SIZE,
          FLAGS_bomb_base_product_size_per_factory
          + FLAGS_bomb_base_product_size_per_factory
          * FLAGS_bomb_tree_num_per_product
          * FLAGS_bomb_base_tree_size
          * FLAGS_bomb_material_per_wip * sizeof(SimpleKey<8>)) != 0) ERR;

      for (const auto& [p_id, root] : bom) {
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
        ItemManufacturingMaster::CreateKey(f_id, p_id, keys[i].ptr());
        tx.lock_entries_.emplace_back(Storage::ItemManufacturingMaster, keys[i].view(), true);
        i++;
        ProductCostMaster::CreateKey(f_id, p_id, keys[i].ptr());
        tx.lock_entries_.emplace_back(Storage::ProductCostMaster, keys[i].view(), true);
        i++;
      }

      // sort keys and lock
      if (!tx.lockList()) {
        tx.unlockList();
        // already reconnaissance fails due to deletion, so retry
        // dump(tx.thid_, "lock fails");
        goto FAIL;
      }

      // access record
      for (const auto& [p_id, root] : bom) {
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
          tx.unlockList();
          tx.status_ = TransactionStatus::invalid;
          return;
        }
      }

      ret = BombWorkload<Tuple,Param>::select_im_by_factory(tx, f_id, verify_products);
      for (auto& p_id : verify_products) {
        Node* root = BombWorkload<Tuple,Param>::build_bom_tree(tx, p_id);
        if (root == nullptr) ERR;
        verify_bom.emplace(p_id, root);
      }
      if (product_ids.size() != verify_products.size()
          || !std::equal(product_ids.cbegin(), product_ids.cend(), verify_products.cbegin())) {
        // dump(tx.thid_, "product verify fails");
        goto FAIL;
      }
      for (const auto& [p_id, root] : bom) {
        std::vector<Node*> nodes;
        std::vector<Node*> verify_nodes;
        Node* verify_root;
        if (auto it = verify_bom.find(p_id); it == verify_bom.end()) {
          // dump(tx.thid_, "bom verify fails (1)");
          goto FAIL;
        } else {
          verify_root = verify_bom.at(p_id);
        }
        root->get_all_nodes(nodes);
        verify_root->get_all_nodes(verify_nodes);
        if (nodes.size() != verify_nodes.size()
            || !std::equal(nodes.cbegin(), nodes.cend(), verify_nodes.cbegin(),
            [](const Node* lhs, const Node* rhs) { return lhs->i_id_ == rhs->i_id_; })) {
          // dump(tx.thid_, "bom verify fails (2)");
          goto FAIL;
        }
      }
      // dump(tx.thid_, "verify done");
      return;

FAIL:
      tx.status_ = TransactionStatus::aborted;
      return;
    }

    Status run_delete_product(TxExecutor& tx, Query& query, uint32_t product_id) {
      Status stat = BombWorkload<Tuple,Param>::delete_item_manufacturing_master(tx, query.args.f_id, product_id);
      if (FLAGS_bomb_interactive_ms) sleepMs(FLAGS_bomb_interactive_ms);
      if (stat != Status::OK) {
        dump(tx.thid_, "delete product fail");
      }
      return stat;
    }

    Status run_change_product(TxExecutor& tx, Query& query) {
RETRY:
      if (tx.quit_) return Status::OK;
      std::vector<uint32_t> product_ids;
      tx.reconnoiter_begin(); // Do target selection as if read from cache
      Status stat = BombWorkload<Tuple,Param>::select_im_by_factory(tx, query.args.f_id, product_ids);
      tx.reconnoiter_end();
      if (stat != Status::OK) {
        return stat;
      }
      uint32_t product_id = BombWorkload<Tuple,Param>::select_random(product_ids);
      SimpleKey<8> key;
      ItemManufacturingMaster::CreateKey(query.args.f_id, product_id, key.ptr());
      tx.lock_entries_.emplace_back(Storage::ItemManufacturingMaster, key.view(), true);

      // sort keys and lock
      if (!tx.lockList()) {
        tx.unlockList();
        // dump(tx.thid_, "WARN: lock fails");
        goto RETRY; // already reconnaissance fails due to deletion, so retry
      }

      // actual transaction logic
      stat = run_delete_product(tx, query, product_id);
      if (stat == Status::OK) {
        // dump(tx.thid_, "INFO: delete OK");
        stat = BombWorkload<Tuple,Param>::run_add_new_prodcut(tx, query);
      } else {
        dump(tx.thid_, "WARN: product deletion fails");
      }
      if (stat != Status::OK) {
        dump(tx.thid_, "WARN: product insertion fails");
      }
      return stat;
    }

    void run_change_raw_material(TxExecutor& tx, Query& query) {
RETRY:
      if (tx.quit_) return;
      std::vector<std::pair<uint32_t,uint32_t>> material_ids;
      tx.reconnoiter_begin(); // Do target selection as if read from cache
      auto ret = BombWorkload<Tuple,Param>::select_materials_by_item_id(tx, query.args.i_id, material_ids);
      tx.reconnoiter_end();
      if (ret != Status::OK) return;

      auto pair = BombWorkload<Tuple,Param>::select_random(material_ids);
      auto parent_id = pair.first;
      auto material_id = pair.second;

      SimpleKey<8> key;
      ItemConstructionMaster::CreateKey(parent_id, material_id, key.ptr());
      tx.lock_entries_.emplace_back(Storage::ItemConstructionMaster, key.view(), true);

      // sort keys and lock
      if (!tx.lockList()) {
        tx.unlockList();
        goto RETRY; // already reconnaissance fails due to deletion, so retry
      }

      // actual transaction logic
      Status stat = BombWorkload<Tuple,Param>::delete_item_construction_master(tx, parent_id, material_id);
      if (FLAGS_bomb_interactive_ms) sleepMs(FLAGS_bomb_interactive_ms);
      if (stat != Status::OK) {
        dump(tx.thid_, "WARN: bom record deletion fails");
        return;
      }
      stat = BombWorkload<Tuple,Param>::insert_item_construction_master(tx, parent_id, query.args.m_id, 10.0);
      if (FLAGS_bomb_interactive_ms) sleepMs(FLAGS_bomb_interactive_ms);
      if (stat != Status::OK && stat != Status::WARN_ALREADY_EXISTS) {
        // record may exist if a material to be inserted has already used for the parent item
        dump(tx.thid_, "WARN: bom record insertion fails");
        return;
      }
    }

    void run_change_product_quantity(TxExecutor& tx, Query& query) {
RETRY:
      if (tx.quit_) return;
      std::vector<uint32_t> product_ids;
      tx.reconnoiter_begin(); // Do target selection as if read from cache
      Status stat = BombWorkload<Tuple,Param>::select_im_by_factory(tx, query.args.f_id, product_ids);
      tx.reconnoiter_end();
      if (stat != Status::OK) return;
      if (product_ids.size() == 0) {
        dump(tx.thid_, "ERROR: No target records.");
        ERR;
      }
      uint32_t product_id = BombWorkload<Tuple,Param>::select_random(product_ids);
      SimpleKey<8> key;
      ItemManufacturingMaster::CreateKey(query.args.f_id, product_id, key.ptr());
      tx.lock_entries_.emplace_back(Storage::ItemManufacturingMaster, key.view(), true);

      // sort keys and lock
      if (!tx.lockList()) {
        tx.unlockList();
        goto RETRY; // already reconnaissance fails due to deletion, so retry
      }

      // actual transaction logic
      stat = BombWorkload<Tuple,Param>::update_item_manufacturing_master(tx, query.args.f_id, product_id, 10.0);
      if (FLAGS_bomb_interactive_ms) sleepMs(FLAGS_bomb_interactive_ms);
      if (stat != Status::OK && stat != Status::WARN_NOT_FOUND) {
        // may not be found if already deleted by S3
        dump(tx.thid_, "WARN: item_manufacturing_master update fails");
        return;
      }
    }

    template <typename TxExecutor, typename TransactionStatus>
    void run(TxExecutor& tx) {
        Query query;
        query.generate(this, tx);
        auto start = std::chrono::high_resolution_clock::now();

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
          case TxType::AddNewProduct:
            run_change_product(tx, query);
            break;
          case TxType::ChangeRawMaterial:
            run_change_raw_material(tx, query);
            break;
          case TxType::ChangeProductQuantity:
            run_change_product_quantity(tx, query);
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

        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        if (!FLAGS_bomb_mixed_mode && FLAGS_bomb_rate_control) {
          auto interval = BombWorkload<Tuple,Param>::get_request_interval(query.type);
          if (elapsed < interval) {
            std::this_thread::sleep_for((interval - elapsed) * 0.95);
          }
        }
        tx.result_->local_latency_per_tx_[get_tx_type(query.type)] =
            tx.result_->local_latency_per_tx_[get_tx_type(query.type)] + elapsed.count();
        return;
    }
};
