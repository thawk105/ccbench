#pragma once

#include <atomic>
#include <thread>
#include <vector>
#include <set>

#include "config.hh"
#include "debug.hh"
#include "heap_object.hh"
#include "procedure.hh"
#include "queue.hh"
#include "random.hh"
#include "result.hh"
#include "tuple_body.hh"
#include "util.hh"
#include "workload.hh"
#include "zipf.hh"

#include "gflags/gflags.h"

#ifdef GLOBAL_VALUE_DEFINE
DEFINE_uint32(bomb_l1_thread_num, 1, "Number of threads for batch (update-product-cost-master) transaction");
DEFINE_uint32(bomb_s1_thread_num, 1, "Number of threads for update-material-cost-master transaction");
DEFINE_uint32(bomb_s2_thread_num, 1, "Number of threads for issue-journal-voucher transaction");
DEFINE_uint32(bomb_s3_thread_num, 0, "Number of threads for add-new-product transaction");
DEFINE_uint32(bomb_s4_thread_num, 0, "Number of threads for change-raw-material transaction");
DEFINE_uint32(bomb_mixed_short_rate, 500, "Request rate of short transactions");
DEFINE_bool(bomb_mixed_short_rate_tps, false, "Use request rate as per-second");
DEFINE_bool(bomb_mixed_mode, false, "Enable mixed-workload mode");
DEFINE_bool(bomb_use_cache, false, "Use cached BoM tree (for static setting)");
DEFINE_bool(bomb_rate_control, false, "Enable rate limit control");
DEFINE_uint32(bomb_l1_rate, 1, "Request rate of L1 transaction");
DEFINE_uint32(bomb_s1_rate, 500, "Request rate of S1 transaction");
DEFINE_uint32(bomb_s2_rate, 100, "Request rate of S2 transaction");
DEFINE_uint32(bomb_s3_rate, 1, "Request rate of S3 transaction");
DEFINE_uint32(bomb_s4_rate, 200, "Request rate of S4 transaction");
DEFINE_uint32(bomb_s5_rate, 10, "Request rate of S5 transaction");
DEFINE_uint64(bomb_perc_s1, 50, "The percentage of S1 transactions");
DEFINE_uint64(bomb_perc_s2, 50, "The percentage of S2 transactions");
DEFINE_uint64(bomb_perc_s3, 0, "The percentage of S3 transactions");
DEFINE_uint64(bomb_perc_s4, 0, "The percentage of S4 transactions");
DEFINE_uint32(bomb_req_batch_size, 1, "Request batch size");
DEFINE_uint32(bomb_factory_size, 8, "Total number of factories");
DEFINE_uint32(bomb_product_size, 72000, "Total number of finished products");
DEFINE_uint32(bomb_work_size, 198000, "Total number of root work in progress (root WIP)");
DEFINE_uint32(bomb_material_size, 75000, "Total number of raw materials");
DEFINE_uint32(bomb_tree_num_per_product, 5, "");
DEFINE_uint32(bomb_base_tree_size, 10, "");
DEFINE_uint32(bomb_material_per_wip, 3, "");
DEFINE_uint32(bomb_base_product_size_per_factory, 100, "");
DEFINE_uint32(bomb_mc_update_size, 1, "Number of update materials in update-material-cost-master transaction");
DEFINE_uint32(bomb_interactive_ms, 0, "Sleep microseconds per SQL(-equivalent) unit");
#else
DECLARE_uint32(bomb_l1_thread_num);
DECLARE_uint32(bomb_s1_thread_num);
DECLARE_uint32(bomb_s2_thread_num);
DECLARE_uint32(bomb_s3_thread_num);
DECLARE_uint32(bomb_s4_thread_num);
DECLARE_uint32(bomb_mixed_short_rate)
DECLARE_bool(bomb_mixed_short_rate_tps)
DECLARE_bool(bomb_mixed_mode)
DECLARE_bool(bomb_use_cache)
DECLARE_bool(bomb_rate_control)
DECLARE_uint32(bomb_l1_rate);
DECLARE_uint32(bomb_s1_rate);
DECLARE_uint32(bomb_s2_rate);
DECLARE_uint32(bomb_s3_rate);
DECLARE_uint32(bomb_s4_rate);
DECLARE_uint32(bomb_s5_rate);
DECLARE_uint32(bomb_perc_s1);
DECLARE_uint32(bomb_perc_s2);
DECLARE_uint32(bomb_perc_s3);
DECLARE_uint32(bomb_perc_s4);
DECLARE_uint32(bomb_short_rate);
DECLARE_uint32(bomb_req_batch_size);
DECLARE_uint32(bomb_factory_size);
DECLARE_uint32(bomb_product_size);
DECLARE_uint32(bomb_work_size);
DECLARE_uint32(bomb_material_size);
DECLARE_uint32(bomb_tree_num_per_product);
DECLARE_uint32(bomb_base_tree_size_size);
DECLARE_uint32(bomb_material_per_wip);
DECLARE_uint32(bomb_base_product_size_per_factory);
DECLARE_uint32(bomb_mc_update_size);
DECLARE_uint32(bomb_interactive_ms);
#endif

typedef std::chrono::high_resolution_clock::time_point timepoint;

GLOBAL std::atomic<uint32_t> ItemIdCounter;
GLOBAL ConcurrentQueue<std::pair<TxType, timepoint>> *requestQueues;

enum class Storage : std::uint32_t {
  ItemConstructionMaster = 0,
  ItemManufacturingMaster,
  MaterialCostMaster,
  ProductCostMaster,
  JournalVoucher,
  ItemMaster,
  Factory,
  Size,
};

enum class TxType : std::uint32_t {
  UpdateProductCostMaster = 0,
  UpdateMaterialCostMaster,
  IssueJournalVoucher,
  AddNewProduct,
  ChangeRawMaterial,
  ChangeProductQuantity,
  Size,
};

struct Factory {
  alignas(CACHE_LINE_SIZE)
  std::uint32_t f_id;

  //Primary Key: key_
  //key size is 8 bytes.
  static void CreateKey(uint32_t f_id, char *out) {
    assign_as_bigendian(f_id, &out[0]);
    assign_as_bigendian(0, &out[4]);
  }

  void createKey(char *out) const { return CreateKey(f_id, out); }

  [[nodiscard]] std::string_view view() const { return struct_str_view(*this); }
};

struct ItemMaster {
  alignas(CACHE_LINE_SIZE)
  std::uint32_t i_id;

  //Primary Key: key_
  //key size is 8 bytes.
  static void CreateKey(uint32_t i_id, char *out) {
    assign_as_bigendian(i_id, &out[0]);
    assign_as_bigendian(0, &out[4]);
  }

  void createKey(char *out) const { return CreateKey(i_id, out); }

  [[nodiscard]] std::string_view view() const { return struct_str_view(*this); }
};

struct ItemConstructionMaster {
  alignas(CACHE_LINE_SIZE)
  std::uint32_t ic_parent_i_id;
  std::uint32_t ic_i_id;
  double ic_material_quantity;

  //Primary Key: key_
  //key size is 8 bytes.
  static void CreateKey(uint32_t ic_parent_i_id, uint32_t ic_i_id, char *out) {
    assign_as_bigendian(ic_parent_i_id, &out[0]);
    assign_as_bigendian(ic_i_id, &out[4]);
  }

  void createKey(char *out) const { return CreateKey(ic_parent_i_id, ic_i_id, out); }

  [[nodiscard]] std::string_view view() const { return struct_str_view(*this); }
};

struct MaterialCostMaster {
  alignas(CACHE_LINE_SIZE)
  std::uint32_t mc_f_id; // factory
  std::uint32_t mc_i_id; // item
  double mc_stock_quantity;
  double mc_stock_price;

  //Primary Key: key_
  //key size is 8 bytes.
  static void CreateKey(uint32_t mc_f_id, uint32_t mc_i_id, char *out) {
    assign_as_bigendian(mc_f_id, &out[0]);
    assign_as_bigendian(mc_i_id, &out[4]);
  }

  void createKey(char *out) const { return CreateKey(mc_f_id, mc_i_id, out); }

  [[nodiscard]] std::string_view view() const { return struct_str_view(*this); }
};

struct ItemManufacturingMaster {
  alignas(CACHE_LINE_SIZE)
  std::uint32_t im_factory_id;
  std::uint32_t im_product_id;
  double im_quantity;

  //Primary Key: key_
  //key size is 8 bytes.
  static void CreateKey(uint32_t im_factory_id, uint32_t im_product_id, char *out) {
    assign_as_bigendian(im_factory_id, &out[0]);
    assign_as_bigendian(im_product_id, &out[4]);
  }

  void createKey(char *out) const { return CreateKey(im_factory_id, im_product_id, out); }

  [[nodiscard]] std::string_view view() const { return struct_str_view(*this); }
};

struct ProductCostMaster {
  alignas(CACHE_LINE_SIZE)
  std::uint32_t pc_factory_id;
  std::uint32_t pc_product_id;
  double pc_cost;

  //Primary Key: key_
  //key size is 8 bytes.
  static void CreateKey(uint32_t pc_factory_id, uint32_t pc_product_id, char *out) {
    assign_as_bigendian(pc_factory_id, &out[0]);
    assign_as_bigendian(pc_product_id, &out[4]);
  }

  void createKey(char *out) const { return CreateKey(pc_factory_id, pc_product_id, out); }

  [[nodiscard]] std::string_view view() const { return struct_str_view(*this); }
};

struct JournalVoucher {
  alignas(CACHE_LINE_SIZE)
  std::uint64_t jv_voucher_id;
  std::uint64_t jv_date;
  std::uint32_t jv_debit;
  std::uint32_t jv_credit;
  double jv_amount;

  //Primary Key: key_
  //key size is 8 bytes.
  static void CreateKey(uint64_t jv_voucher_id, char *out) {
    assign_as_bigendian(jv_voucher_id, &out[0]);
  }

  void createKey(char *out) const { return CreateKey(jv_voucher_id, out); }

  [[nodiscard]] std::string_view view() const { return struct_str_view(*this); }
};

class Node {
public:
  Node* parent_ = nullptr;
  std::vector<Node*> childs_;
  uint32_t i_id_;
  double quantity_;
  double total_cost_ = 0;
  double unit_cost_ = 0; // update baed on MaterialCostMaster when necessary

  Node() {}
  Node(uint32_t i_id) : i_id_(i_id), quantity_(1.0) {}
  Node(uint32_t i_id, double q) : i_id_(i_id), quantity_(q) {}

  void add_child(Node* child) {
    child->parent_ = this;
    childs_.push_back(child);
  }

  void assign_id(std::atomic<uint32_t>& i_id) {
    // skip root WIP to assign i_id in reserved range (<= FLAGS_bomb_work_size)
    if (parent_ != nullptr)
      i_id_ = i_id++;
    for (auto& child : childs_) {
      child->assign_id(i_id);
    }
  }

  bool is_leaf() {
    return childs_.empty();
  }

  void get_all_nodes(std::vector<Node*>& nodes) {
    collect_nodes(nodes);
  }

  void collect_nodes(std::vector<Node*>& nodes) {
    nodes.push_back(this);
    for (auto& child : childs_) {
      child->collect_nodes(nodes);
    }
  }

  void get_all_leafs(std::vector<Node*>& nodes) {
    for (auto& child : childs_) {
      child->collect_leafs(nodes);
    }
  }

  void collect_leafs(std::vector<Node*>& nodes) {
    if (this->is_leaf()) {
      nodes.push_back(this);
    }
    for (auto& child : childs_) {
      child->collect_nodes(nodes);
    }
  }

  double calculate_cost() {
    if (this->is_leaf()) {
      // std::cout << "leaf: [" << i_id_ << "] " << unit_cost*quantity_ << std::endl;
      return unit_cost_ * quantity_;
    }
    double subtotal = 0;
    for (auto& child : childs_) {
      subtotal += child->calculate_cost();
    }
    // std::cout << "subtotal: [" << i_id_ << "] " << subtotal << std::endl;
    return subtotal * quantity_;
  }
};

struct JVID {
  union {
    uint64_t obj_;
    struct {
      uint64_t thid: 8;
      uint64_t vid: 56;
    };
  };

  JVID() : obj_(0) {};
};

template <typename Tuple, typename Param>
class BombWorkload {
public:
    Param* param_;
    Xoroshiro128Plus rnd_;
    uint64_t jv_counter_ = 0;
    std::vector<uint32_t> s3_counters_; // to stabilize # of target products for L1
    uint32_t i_id_wip_end_;
    uint32_t s5_thread_num;
    std::map<uint32_t,Node*> bom_cache_;

    BombWorkload() {
        rnd_.init();

        if (!FLAGS_bomb_mixed_mode) {
          int total_threads = FLAGS_bomb_l1_thread_num
                              + FLAGS_bomb_s1_thread_num
                              + FLAGS_bomb_s2_thread_num
                              + FLAGS_bomb_s3_thread_num
                              + FLAGS_bomb_s4_thread_num;
          if (FLAGS_bomb_l1_thread_num && FLAGS_thread_num < total_threads) {
            std::cerr << "Total number of threads must be " << FLAGS_thread_num << std::endl;
            ERR;
          }
          s5_thread_num = FLAGS_thread_num - (
              FLAGS_bomb_l1_thread_num
              + FLAGS_bomb_s1_thread_num
              + FLAGS_bomb_s2_thread_num
              + FLAGS_bomb_s3_thread_num
              + FLAGS_bomb_s4_thread_num);
        }

        s3_counters_.resize(FLAGS_bomb_factory_size, 0);
    }

    static std::chrono::nanoseconds get_request_interval_nano() {
      // use FLAGS_bomb_mixed_short_rate as request per seconds
      return std::chrono::nanoseconds(1000*1000*1000/FLAGS_bomb_mixed_short_rate);
    }

    static std::chrono::microseconds get_request_interval_micro() {
      // use FLAGS_bomb_mixed_short_rate as request per minute
      return std::chrono::microseconds(60*1000*1000/FLAGS_bomb_mixed_short_rate);
    }

    std::chrono::microseconds get_request_interval(TxType type) {
      uint32_t rate;
      switch (type) {
        case TxType::UpdateProductCostMaster:
          // L1
          rate = FLAGS_bomb_l1_rate / FLAGS_bomb_l1_thread_num;
          break;
        case TxType::UpdateMaterialCostMaster:
          // S1
          rate = FLAGS_bomb_s1_rate / FLAGS_bomb_s1_thread_num;
          break;
        case TxType::IssueJournalVoucher:
          // S2
          rate = FLAGS_bomb_s2_rate / FLAGS_bomb_s2_thread_num;
          break;
        case TxType::AddNewProduct:
          // S3
          rate = FLAGS_bomb_s3_rate / FLAGS_bomb_s3_thread_num;
          break;
        case TxType::ChangeRawMaterial:
          // S4
          rate = FLAGS_bomb_s4_rate / FLAGS_bomb_s4_thread_num;
          break;
        case TxType::ChangeProductQuantity:
          // S5
          rate = FLAGS_bomb_s5_rate / s5_thread_num;
          break;
        default:
          ERR;
          break;
      }
      return std::chrono::microseconds(60*1000*1000/rate);
    }

    class TxArgs {
    public:
      uint32_t f_id;
      uint32_t p_id;
      uint32_t i_id;
      uint32_t m_id;
      std::set<uint32_t> i_id_set;
      bool add;

      void generate(BombWorkload* w, TxExecutor& tx, TxType type) {
        switch (type) {
          case TxType::IssueJournalVoucher:
          case TxType::UpdateProductCostMaster:
            f_id = w->rnd_.random_int(1, FLAGS_bomb_factory_size);
            break;
          case TxType::UpdateMaterialCostMaster:
            f_id = w->rnd_.random_int(1, FLAGS_bomb_factory_size);
            while (i_id_set.size() < FLAGS_bomb_mc_update_size) {
              uint32_t m_id = w->rnd_.random_int(
                                get_i_id_material_start(),
                                get_i_id_material_start() + FLAGS_bomb_material_size - 1);
              i_id_set.emplace(m_id);
            }
            break;
          case TxType::AddNewProduct:
            f_id = w->rnd_.random_int(1, FLAGS_bomb_factory_size);
            p_id = ItemIdCounter.fetch_add(1) + 1;
            while (i_id_set.size() < FLAGS_bomb_tree_num_per_product) {
              uint32_t i_id = w->rnd_.random_int(
                                get_i_id_work_start(),
                                get_i_id_work_start() + FLAGS_bomb_work_size - 1);
              i_id_set.emplace(i_id);
            }
#ifdef ADD_OR_DELETE
            // if switch add and delete in turn for each factory
            add = w->s3_counters_.at(f_id-1) % 2 == 0 ? true : false;
            if (add) {
              p_id = ItemIdCounter.fetch_add(1) + 1;
              while (i_id_set.size() < FLAGS_bomb_tree_num_per_product) {
                uint32_t i_id = w->rnd_.random_int(
                                  get_i_id_work_start(),
                                  get_i_id_work_start() + FLAGS_bomb_work_size - 1);
                i_id_set.emplace(i_id);
              }
            }
            w->s3_counters_.at(f_id-1)++;
#endif
            break;
          case TxType::ChangeRawMaterial:
            // get a root_wip item id randomly (deleted item is selected from the tree)
            i_id = w->rnd_.random_int(
                get_i_id_work_start(),
                get_i_id_work_start() + FLAGS_bomb_work_size);
            // get a material item id to insert
            m_id = w->rnd_.random_int(
                get_i_id_material_start(),
                get_i_id_material_start() + FLAGS_bomb_material_size - 1);
            break;
          case TxType::ChangeProductQuantity:
            f_id = w->rnd_.random_int(1, FLAGS_bomb_factory_size);
            break;
          default:
            if (loadAcquire(tx.quit_)) break;
            ERR;
        }
      }
    };

    class Query {
    public:
      TxType type;
      TxArgs args;

      TxType decideType(TxExecutor& tx, Xoroshiro128Plus& r) {
        TxType txType;
        if (tx.thid_ < FLAGS_bomb_l1_thread_num) {
          txType = TxType::UpdateProductCostMaster;
        } else if (tx.thid_ < FLAGS_bomb_l1_thread_num
                              + FLAGS_bomb_s1_thread_num) {
          txType = TxType::UpdateMaterialCostMaster;
        } else if (tx.thid_ < FLAGS_bomb_l1_thread_num
                              + FLAGS_bomb_s1_thread_num
                              + FLAGS_bomb_s2_thread_num) {
          txType = TxType::IssueJournalVoucher;
        } else if (tx.thid_ < FLAGS_bomb_l1_thread_num
                              + FLAGS_bomb_s1_thread_num
                              + FLAGS_bomb_s2_thread_num
                              + FLAGS_bomb_s3_thread_num) {
          txType = TxType::AddNewProduct;
        } else if (tx.thid_ < FLAGS_bomb_l1_thread_num
                              + FLAGS_bomb_s1_thread_num
                              + FLAGS_bomb_s2_thread_num
                              + FLAGS_bomb_s3_thread_num
                              + FLAGS_bomb_s4_thread_num) {
          txType = TxType::ChangeRawMaterial;
        } else {
          txType = TxType::ChangeProductQuantity;
        }

        return txType;
      }

      std::pair<TxType,timepoint> getRequest(TxExecutor& tx) {
        if (tx.thid_ < FLAGS_bomb_l1_thread_num) {
          timepoint start = std::chrono::high_resolution_clock::now();;
          return make_pair(TxType::UpdateProductCostMaster, start);
        } else {
          int queueIndex = tx.thid_ - FLAGS_bomb_l1_thread_num;
          std::pair<TxType, timepoint> ret;
          while (!loadAcquire(tx.quit_)) {
            try {
              ret = requestQueues[queueIndex].pop();
              break;
            } catch (std::out_of_range e) {
              std::this_thread::sleep_for(std::chrono::nanoseconds(100));
            }
          }
          return ret;
        }
      }

      timepoint generate(BombWorkload* workload, TxExecutor& tx) {
        timepoint start;
        if (tx.thid_ < FLAGS_bomb_l1_thread_num) {
          type = TxType::UpdateProductCostMaster;
          start = std::chrono::high_resolution_clock::now();
        } else  if (!FLAGS_bomb_mixed_mode) {
          type = decideType(tx, workload->rnd_);
          start = std::chrono::high_resolution_clock::now();
        } else {
          auto ret = getRequest(tx);
          type = ret.first;
          start = ret.second;
        }
        args.generate(workload, tx, type);
        return start;
      }
    };

    static const uint32_t get_i_id_product_start() {
      return 1;
    }

    static const uint32_t get_i_id_material_start() {
      return get_i_id_product_start() + FLAGS_bomb_product_size;
    }

    static const uint32_t get_i_id_work_start() {
      return get_i_id_material_start() + FLAGS_bomb_material_size;
    }

    template<typename S>
    static inline auto select_random(Xoroshiro128Plus& r, const S &s) {
      assert(!s.empty());
      auto itr = std::begin(s);
      std::advance(itr, r.random_int(0, s.size()-1));
      return itr;
    }

    template<typename T>
    inline T select_random(const std::set<T> &s) {
      assert(!s.empty());
      auto itr = std::begin(s);
      std::advance(itr, rnd_.random_int(0, s.size()-1));
      return *itr;
    }

    template<typename T>
    inline T select_random(const std::vector<T> &v) {
      assert(!v.empty());
      auto itr = std::begin(v);
      std::advance(itr, rnd_.random_int(0, v.size()-1));
      return *itr;
    }

    void prepare(TxExecutor& tx, Param *p) {
      // for CC specific initialization parameter
      this->param_ = p;
      this->i_id_wip_end_ = ItemIdCounter.load();

      if (FLAGS_bomb_use_cache) {
        tx.reconnoiter_begin();
        std::vector<uint32_t> product_ids;
        auto ret = BombWorkload<Tuple,Param>::select_im_by_factory(tx, 1, product_ids);
        for (auto& p_id : product_ids) {
          Node* root = BombWorkload<Tuple,Param>::build_bom_tree(tx, p_id);
          if (root == nullptr) ERR;
          bom_cache_.emplace(p_id, root);
        }
        tx.reconnoiter_end();
        dump(tx.thid_, "[INFO] BoM cache creation done");
      }
    }

    Status update_product_cost_master(TxExecutor& tx, uint32_t f_id, uint32_t p_id, double cost) {
      SimpleKey<8> key;
      ProductCostMaster::CreateKey(f_id, p_id, key.ptr());
      HeapObject obj;
      obj.allocate<ProductCostMaster>();
      ProductCostMaster& pc = obj.ref();
      pc.pc_factory_id = f_id;
      pc.pc_product_id = p_id;
      pc.pc_cost = cost;
      Status status = tx.write(Storage::ProductCostMaster, key.view(), TupleBody(key.view(), std::move(obj)));
      if (FLAGS_bomb_interactive_ms) sleepMicroSec(FLAGS_bomb_interactive_ms);
      return status;
    }

    Node* build_bom_tree(TxExecutor& tx, uint32_t id) {
      SimpleKey<8> item, low, up;
      std::vector<TupleBody*> result;
      Node* root = new Node(id);
      std::vector<Node*> next;
      next.push_back(root);
      while (next.size() != 0) {
        auto node = next.back();
        next.pop_back();
        auto n = tx.read_set_.size();
        ItemMaster::CreateKey(node->i_id_, item.ptr());
        Status stat = tx.read_lock(Storage::ItemMaster, item.view());
        if (stat != Status::OK) {
          std:stringstream ss; ss << "item id: " << node->i_id_;
          dump(tx.thid_, ss.str());
          return nullptr;
        }
        ItemConstructionMaster::CreateKey(node->i_id_, 0, low.ptr());
        ItemConstructionMaster::CreateKey(node->i_id_ + 1, 0, up.ptr());
        tx.scan(Storage::ItemConstructionMaster, low.view(), false, up.view(), true, result);
        if (FLAGS_bomb_interactive_ms && !tx.reconnoitering_) sleepMicroSec(FLAGS_bomb_interactive_ms);
        if (tx.status_ == TransactionStatus::aborted) {
          dump(tx.thid_, "scan failed");
          return nullptr; // TODO: clean up tree
        }
        for (auto& tuple : result) {
          const ItemConstructionMaster& ic = tuple->get_value().cast_to<ItemConstructionMaster>();
          // std::cout << " PID: " << ic.ic_parent_i_id
          //           << " ID: " << ic.ic_i_id
          //           << " Q: " << ic.ic_material_quantity << std::endl;
          Node* n = new Node(ic.ic_i_id, ic.ic_material_quantity);
          node->add_child(n);
          next.push_back(n);
        }
      }
      return root;
    }

    void insert_journal_voucher(TxExecutor& tx, uint32_t debit, uint32_t credit, double amount) {
      JVID jv_id;
      jv_id.thid = tx.thid_;
      jv_id.vid = jv_counter_++;
      SimpleKey<8> key;
      JournalVoucher::CreateKey(jv_id.obj_, key.ptr());
      HeapObject obj;
      obj.allocate<JournalVoucher>();
      JournalVoucher& jv_tuple = obj.ref();
      jv_tuple.jv_voucher_id = jv_id.obj_;
      jv_tuple.jv_date = 20220101235959;
      jv_tuple.jv_debit = debit;
      jv_tuple.jv_credit = credit;
      jv_tuple.jv_amount = amount;
      tx.insert(Storage::JournalVoucher, key.view(), TupleBody(key.view(), std::move(obj)));
    }

    void run_issue_journal_voucher(TxExecutor& tx, Query& query) {
      uint32_t f_id = query.args.f_id;
      SimpleKey<8> factory, low, up;
      std::vector<TupleBody*> result;
      Factory::CreateKey(f_id, factory.ptr());
      ProductCostMaster::CreateKey(f_id, 0, low.ptr());
      ProductCostMaster::CreateKey(f_id+1, 0, up.ptr());
      Status stat = tx.read_lock(Storage::Factory, factory.view());
      if (stat != Status::OK) return;
      tx.scan(Storage::ProductCostMaster, low.view(), false, up.view(), true, result);
      if (FLAGS_bomb_interactive_ms) sleepMicroSec(FLAGS_bomb_interactive_ms);
      if (tx.status_ == TransactionStatus::aborted) return;

      uint32_t debit = 0;  // product
      uint32_t credit = 1; // work in progress
      for (auto& tuple : result) {
        const ProductCostMaster& pm = tuple->get_value().cast_to<ProductCostMaster>();
        double manufactured_quantity = 99.9;
        insert_journal_voucher(tx, debit, credit, manufactured_quantity*pm.pc_cost);
        if (tx.status_ == TransactionStatus::aborted) return;
      }
      if (FLAGS_bomb_interactive_ms) sleepMicroSec(FLAGS_bomb_interactive_ms); // as batch
    }

    void run_update_material_cost_master(TxExecutor& tx, Query& query) {
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
      if (FLAGS_bomb_interactive_ms) sleepMicroSec(FLAGS_bomb_interactive_ms); // as batch
    }

    void _run_update_product_cost_master_using_cache(TxExecutor& tx, Query& query) {
      uint32_t f_id = query.args.f_id;
      std::map<uint32_t,double> costs;
      for (const auto& [p_id, root] : bom_cache_) {
        std::vector<Node*> nodes;
        root->get_all_nodes(nodes);
        for (auto& node : nodes) {
          if (node->is_leaf()) {
            if (!get_material_cost(tx, f_id, node->i_id_, node->unit_cost_))
              return;
          }
        }
        costs.emplace(p_id, root->calculate_cost());
      }
      for (const auto& [p_id, cost] : costs) {
        update_product_cost_master(tx, f_id, p_id, cost);
        if (tx.status_ == TransactionStatus::aborted) return;
        if (tx.quit_) { // for long read phase
          tx.abort();
          tx.status_ = TransactionStatus::invalid;
          return;
        }
      }
    }

    void _run_update_product_cost_master(TxExecutor& tx, Query& query) {
      uint32_t f_id = query.args.f_id;
      std::vector<uint32_t> product_ids;
      SimpleKey<8> factory;
      Factory::CreateKey(f_id, factory.ptr());
      Status stat = tx.read_lock(Storage::Factory, factory.view());
      if (stat != Status::OK) return;
      stat = select_im_by_factory(tx, f_id, product_ids);
      if (FLAGS_bomb_interactive_ms) sleepMicroSec(FLAGS_bomb_interactive_ms);
      if (stat != Status::OK) return;
      if (product_ids.size() == 0) {
        dump(tx.thid_, "ERROR: No target records.");
        ERR;
      }

      std::map<uint32_t,double> costs;
      for (auto& p_id : product_ids) {
        Node* root = build_bom_tree(tx, p_id);
        if (root == nullptr) {
          if (tx.status_ == TransactionStatus::aborted) {
            return;
          } else {
            ERR;
          }
        }
        std::vector<Node*> nodes;
        root->get_all_nodes(nodes);
        for (auto& node : nodes) {
          if (node->is_leaf()) {
            if (!get_material_cost(tx, f_id, node->i_id_, node->unit_cost_))
              return;
          }
        }
        costs.emplace(p_id, root->calculate_cost());
      }
      for (const auto& [p_id, cost] : costs) {
        Status status = update_product_cost_master(tx, f_id, p_id, cost);
        if (status == Status::WARN_NOT_FOUND) {
          status = insert_product_cost_master(tx, f_id, p_id, 0.0);
          if (FLAGS_bomb_interactive_ms) sleepMicroSec(FLAGS_bomb_interactive_ms);
          if (stat != Status::OK) {
            std::stringstream ss;
            ss << "ERROR: insert product cost master fails " << f_id << " " << p_id;
            dump(tx.thid_, ss.str());
            ERR;
          }
        }
        if (tx.status_ == TransactionStatus::aborted) return;
        if (tx.quit_) { // for long read phase
          tx.abort();
          tx.status_ = TransactionStatus::invalid;
          return;
        }
      }
    }

    void run_update_product_cost_master(TxExecutor& tx, Query& query) {
      if (FLAGS_bomb_use_cache) {
        _run_update_product_cost_master_using_cache(tx, query);
      } else {
        _run_update_product_cost_master(tx, query);
      }
    }

    Status insert_item_master(TxExecutor& tx, uint32_t i_id) {
      SimpleKey<8> key;
      ItemMaster::CreateKey(i_id, key.ptr());
      HeapObject obj;
      obj.allocate<ItemMaster>();
      ItemMaster& i_tuple = obj.ref();
      i_tuple.i_id = i_id;
      return tx.insert(Storage::ItemMaster, key.view(), TupleBody(key.view(), std::move(obj)));
    }

    Status insert_item_construction_master(TxExecutor& tx,
                                           uint32_t parent_id, uint32_t id, double material_quantity) {
      SimpleKey<8> key;
      ItemConstructionMaster::CreateKey(parent_id, id, key.ptr());
      HeapObject obj;
      obj.allocate<ItemConstructionMaster>();
      ItemConstructionMaster& ic_tuple = obj.ref();
      ic_tuple.ic_parent_i_id = parent_id;
      ic_tuple.ic_i_id = id;
      ic_tuple.ic_material_quantity = material_quantity;
      return tx.insert(Storage::ItemConstructionMaster, key.view(), TupleBody(key.view(), std::move(obj)));
    }

    Status insert_item_manufacturing_master(TxExecutor& tx,
                                            uint32_t factory_id, uint32_t product_id, double quantity) {
      SimpleKey<8> key;
      ItemManufacturingMaster::CreateKey(factory_id, product_id, key.ptr());
      HeapObject obj;
      obj.allocate<ItemManufacturingMaster>();
      ItemManufacturingMaster& im_tuple = obj.ref();
      im_tuple.im_factory_id = factory_id;
      im_tuple.im_product_id = product_id;
      im_tuple.im_quantity = quantity;
      return tx.insert(Storage::ItemManufacturingMaster, key.view(), TupleBody(key.view(), std::move(obj)));
    }

    Status insert_product_cost_master(TxExecutor& tx, uint32_t f_id, uint32_t p_id, double cost) {
      SimpleKey<8> key;
      ProductCostMaster::CreateKey(f_id, p_id, key.ptr());
      HeapObject obj;
      obj.allocate<ProductCostMaster>();
      ProductCostMaster& p_tuple = obj.ref();
      p_tuple.pc_factory_id = f_id;
      p_tuple.pc_product_id = p_id;
      p_tuple.pc_cost = cost;
      return tx.insert(Storage::ProductCostMaster, key.view(), TupleBody(key.view(), std::move(obj)));
    }

    Status delete_item_construction_master(TxExecutor& tx, uint32_t parent_id, uint32_t material_id) {
      SimpleKey<8> key;
      ItemConstructionMaster::CreateKey(parent_id, material_id, key.ptr());
      return tx.delete_record(Storage::ItemConstructionMaster, key.view());
    }

    Status delete_item_manufacturing_master(TxExecutor& tx, uint32_t factory_id, uint32_t product_id) {
      SimpleKey<8> key;
      ItemManufacturingMaster::CreateKey(factory_id, product_id, key.ptr());
      return tx.delete_record(Storage::ItemManufacturingMaster, key.view());
    }

    Status update_item_manufacturing_master(TxExecutor& tx,
                                            uint32_t factory_id, uint32_t product_id, double quantity) {
      SimpleKey<8> key;
      ItemManufacturingMaster::CreateKey(factory_id, product_id, key.ptr());
      HeapObject obj;
      obj.allocate<ItemManufacturingMaster>();
      ItemManufacturingMaster& im_tuple = obj.ref();
      im_tuple.im_factory_id = factory_id;
      im_tuple.im_product_id = product_id;
      im_tuple.im_quantity = quantity;
      return tx.write(Storage::ItemManufacturingMaster, key.view(), TupleBody(key.view(), std::move(obj)));
    }

    Status upsert_item_manufacturing_master(TxExecutor& tx,
                                            uint32_t factory_id, uint32_t product_id, double quantity) {
      SimpleKey<8> key;
      ItemManufacturingMaster::CreateKey(factory_id, product_id, key.ptr());
      TupleBody* body;

      HeapObject obj;
      obj.allocate<ItemManufacturingMaster>();
      ItemManufacturingMaster& im = obj.ref();
      im.im_factory_id = factory_id;
      im.im_product_id= product_id;
      im.im_quantity = quantity;

      Status stat = tx.read(Storage::ItemManufacturingMaster, key.view(), &body);
      if (FLAGS_bomb_interactive_ms) sleepMicroSec(FLAGS_bomb_interactive_ms);
      if (tx.status_ == TransactionStatus::aborted) return stat;
      if (stat == Status::WARN_NOT_FOUND) {
        // insert
        stat = tx.insert(Storage::ItemManufacturingMaster, key.view(), TupleBody(key.view(), std::move(obj)));
      } else {
        // update
        stat = tx.write(Storage::ItemManufacturingMaster, key.view(), TupleBody(key.view(), std::move(obj)));
      }
      if (FLAGS_bomb_interactive_ms) sleepMicroSec(FLAGS_bomb_interactive_ms);
      return stat;
    }

    Status update_item_construction_master(TxExecutor& tx,
        uint32_t parent_id, uint32_t item_id, double quantity) {
      SimpleKey<8> key;
      ItemConstructionMaster::CreateKey(parent_id, item_id, key.ptr());
      HeapObject obj;
      obj.allocate<ItemConstructionMaster>();
      ItemConstructionMaster& ic_tuple = obj.ref();
      ic_tuple.ic_parent_i_id = parent_id;
      ic_tuple.ic_i_id = item_id;
      ic_tuple.ic_material_quantity = quantity;
      return tx.write(Storage::ItemConstructionMaster, key.view(), TupleBody(key.view(), std::move(obj)));
    }

    Status run_add_new_prodcut(TxExecutor& tx, Query& query) {
      Status stat;
      uint32_t f_id = query.args.f_id;
      uint32_t p_id = query.args.p_id;
      for (auto& i_id : query.args.i_id_set) {
        stat = insert_item_construction_master(tx, p_id, i_id, 1.0);
        if (stat != Status::OK) {
          std::stringstream ss;
          ss << "WARN: insert bom table fails " << f_id << " " << i_id;
          dump(tx.thid_, ss.str());
          return stat;
        }
      }

      stat = insert_item_manufacturing_master(tx, f_id, p_id, 10.0);
      if (stat != Status::OK) {
        std::stringstream ss;
        ss << "WARN: insert product table fails " << f_id << " " << p_id;
        dump(tx.thid_, ss.str());
      }

      // insert item master
      stat = insert_item_master(tx, p_id);
      if (FLAGS_bomb_interactive_ms) sleepMicroSec(FLAGS_bomb_interactive_ms);
      if (stat != Status::OK) {
        std::stringstream ss;
        ss << "WARN: insert item master fails " << f_id << " " << p_id;
        dump(tx.thid_, ss.str());
      }

      return stat;
    }

    Status run_delete_product(TxExecutor& tx, Query& query) {
      std::vector<uint32_t> product_ids;
      tx.reconnoiter_begin(); // Do target selection as if read from cache
      Status stat = select_im_by_factory(tx, query.args.f_id, product_ids);
      tx.reconnoiter_end();
      if (stat != Status::OK) return stat;

      uint32_t product_id = select_random(product_ids);
      stat = delete_item_manufacturing_master(tx, query.args.f_id, product_id);
      if (FLAGS_bomb_interactive_ms) sleepMicroSec(FLAGS_bomb_interactive_ms);
      if (stat != Status::OK) {
        dump(tx.thid_, "delete product fail");
      }
      return stat;
    }

    void run_add_or_delete_product(TxExecutor& tx, Query& query) {
      if (query.args.add) {
        run_add_new_prodcut(tx, query);
      } else {
        run_delete_product(tx, query);
      }
    }

    void run_change_product(TxExecutor& tx, Query& query) {
      SimpleKey<8> factory;
      Factory::CreateKey(query.args.f_id, factory.ptr());
      Status stat = tx.write_lock(Storage::Factory, factory.view());
      if (stat != Status::OK) {
        return;
      }
      stat = run_delete_product(tx, query);
      if (stat != Status::OK) {
        dump(tx.thid_, "delete product fail");
        return;
      } else {
        run_add_new_prodcut(tx, query);
      }
    }

    Status run_add_raw_material(TxExecutor& tx, Query& query) {
      Status stat = insert_item_construction_master(tx, query.args.i_id, query.args.m_id, 1.0);
      if (FLAGS_bomb_interactive_ms) sleepMicroSec(FLAGS_bomb_interactive_ms);
      return stat;
    }

    Status run_delete_raw_material(TxExecutor& tx, Query& query) {
      std::vector<std::pair<uint32_t,uint32_t>> material_ids;
      tx.reconnoiter_begin(); // Do target selection as if read from cache
      auto ret = select_materials_by_item_id(tx, query.args.i_id, material_ids);
      tx.reconnoiter_end();
      if (ret != Status::OK) return ret;

      if (material_ids.size() == 1) {
        dump(tx.thid_, "WARN: No target records, but continue by adding a new material");
        return run_add_raw_material(tx, query);
      }

      auto pair = select_random(material_ids);
      auto parent_id = pair.first;
      auto material_id = pair.second;
      Status stat = delete_item_construction_master(tx, parent_id, material_id);
      if (FLAGS_bomb_interactive_ms) sleepMicroSec(FLAGS_bomb_interactive_ms);
        return stat;
    }

    Status run_add_or_delete_raw_material(TxExecutor& tx, Query& query) {
      if (query.args.add) {
        return run_add_raw_material(tx, query);
      } else {
        return run_delete_raw_material(tx, query);
      }
    }

    void run_change_raw_material(TxExecutor& tx, Query& query) {
      std::vector<std::pair<uint32_t,uint32_t>> material_ids;
      tx.reconnoiter_begin(); // Do target selection as if read from cache
      auto ret = select_materials_by_item_id(tx, query.args.i_id, material_ids);
      tx.reconnoiter_end();
      if (ret != Status::OK) return;
 
      auto pair = select_random(material_ids);
      auto parent_id = pair.first;
      auto material_id = pair.second;
      SimpleKey<8> item;
      ItemMaster::CreateKey(parent_id, item.ptr());
      Status stat = tx.write_lock(Storage::ItemMaster, item.view());
      if (stat != Status::OK) return;
      stat = delete_item_construction_master(tx, parent_id, material_id);
      if (FLAGS_bomb_interactive_ms) sleepMicroSec(FLAGS_bomb_interactive_ms);
      if (stat != Status::OK) return;
      insert_item_construction_master(tx, parent_id, query.args.m_id, 10.0);
      if (FLAGS_bomb_interactive_ms) sleepMicroSec(FLAGS_bomb_interactive_ms);
    }

    void run_change_product_quantity(TxExecutor& tx, Query& query) {
      std::vector<uint32_t> product_ids;
      tx.reconnoiter_begin(); // Do target selection as if read from cache
      Status stat = select_im_by_factory(tx, query.args.f_id, product_ids);
      tx.reconnoiter_end();
      // stringstream ss; ss << "[S5] f_id: " << query.args.f_id << " products: " << product_ids.size();
      // dump(tx.thid_, ss.str());
      if (stat != Status::OK) return;
      if (product_ids.size() == 0) {
        dump(tx.thid_, "ERROR: No target records.");
        ERR;
      }

      uint32_t product_id = select_random(product_ids);
      update_item_manufacturing_master(tx, query.args.f_id, product_id, 10.0);
      if (FLAGS_bomb_interactive_ms) sleepMicroSec(FLAGS_bomb_interactive_ms);
    }

    bool get_material_cost(TxExecutor& tx, uint32_t f_id, uint32_t m_id, double& cost) {
      SimpleKey<8> key;
      MaterialCostMaster::CreateKey(f_id, m_id, key.ptr());
      TupleBody* body;
      Status stat = tx.read(Storage::MaterialCostMaster, key.view(), &body);
      if (tx.status_ == TransactionStatus::aborted) return false;
      MaterialCostMaster& mc = body->get_value().cast_to<MaterialCostMaster>();
      cost = mc.mc_stock_price / mc.mc_stock_quantity;
      return true;
    }

    Status select_materials_by_item_id(TxExecutor& tx, uint32_t i_id,
        std::vector<std::pair<uint32_t,uint32_t>>& material_ids) {
      // WIP with i_id may not have leaf nodes (materials),
      // so recursively scan and get all materials
      Node* parent = build_bom_tree(tx, i_id);
      if (parent == nullptr) ERR; // TODO: retry?

      std::vector<Node*> nodes;
      parent->get_all_leafs(nodes);

      if (nodes.size() == 0) ERR; // TODO: retry?
      for (auto& node : nodes) {
        material_ids.emplace_back(node->parent_->i_id_, node->i_id_);       
      }
      return Status::OK;
    }

    void select_ic_by_product(TxExecutor& tx, uint32_t p_id) {
      // for debug
      SimpleKey<8> low, up;
      std::vector<TupleBody*> result;
      ItemConstructionMaster::CreateKey(p_id, 0, low.ptr());
      ItemConstructionMaster::CreateKey(p_id+1, 0, up.ptr());
      tx.scan(Storage::ItemConstructionMaster, low.view(), false, up.view(), false, result);
      for (auto& tuple : result) {
        const ItemConstructionMaster& ic = tuple->get_value().cast_to<ItemConstructionMaster>();
        std::cout << " PID: " << ic.ic_parent_i_id
                  << " ID: " << ic.ic_i_id
                  << " Q: " << ic.ic_material_quantity << std::endl;
      }
    }

    Status select_im_by_factory(TxExecutor& tx, uint32_t factory_id, std::vector<uint32_t>& product_ids) {
      SimpleKey<8> low, up;
      std::vector<TupleBody*> result;
      ItemManufacturingMaster::CreateKey(factory_id, 0, low.ptr());
      ItemManufacturingMaster::CreateKey(factory_id+1, 0, up.ptr());
      Status stat = tx.scan(Storage::ItemManufacturingMaster, low.view(), false, up.view(), true, result);
      if (tx.status_ == TransactionStatus::aborted) {
        stringstream ss; ss << "[common] failed to scan manufactured products f_id: " << factory_id;
        dump(tx.thid_, ss.str());
        return stat;
      }
      for (auto& tuple : result) {
        const ItemManufacturingMaster& im = tuple->get_value().cast_to<ItemManufacturingMaster>();
        product_ids.emplace_back(im.im_product_id);
      }
      return stat;
    }

    void select_all_im_master(TxExecutor& tx) {
      // for debug
      SimpleKey<8> low, up;
      std::vector<TupleBody*> result;
      ItemManufacturingMaster::CreateKey(1, 0, low.ptr());
      ItemManufacturingMaster::CreateKey(FLAGS_bomb_factory_size, FLAGS_bomb_product_size, up.ptr());
      tx.scan(Storage::ItemManufacturingMaster, low.view(), false, up.view(), false, result);
      for (auto& tuple : result) {
        const ItemManufacturingMaster& im = tuple->get_value().cast_to<ItemManufacturingMaster>();
        std::cout << " FID: " << im.im_factory_id
                  << " PID: " << im.im_product_id
                  << " Q: " << im.im_quantity << std::endl;
      }
    }

    // void dump_all_ic_master() {
    //   // for debug (single-version CC only)
    //   SimpleKey<8> low, up;
    //   std::vector<TupleBody*> result;
    //   ItemConstructionMaster::CreateKey(1, 0, low.ptr());
    //   ItemConstructionMaster::CreateKey(10000000, 0, up.ptr());
    //   std::vector<Tuple*> scan_res;
    //   Masstrees[get_storage(Storage::ItemConstructionMaster)].scan(
    //       low.view().data(), low.view().size(), false,
    //       up.view().data(), up.view().size(), true, 
    //       &scan_res, false);
    //   for (auto& tuple : scan_res) {
    //     const ItemConstructionMaster& ic = tuple->body_.get_value().cast_to<ItemConstructionMaster>();
    //     std::cout << " PID: " << ic.ic_parent_i_id
    //               << " ID: " << ic.ic_i_id
    //               << " Q: " << ic.ic_material_quantity
    //               << " TUPLE: " << tuple << std::endl;
    //   }
    // }

    template <typename TxExecutor, typename TransactionStatus>
    void run(TxExecutor& tx) {
        Status stat;
        Query query;
        auto start = query.generate(this, tx);

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
            run_update_product_cost_master(tx, query);
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
          auto interval = get_request_interval(query.type);
          if (elapsed < interval) {
            std::this_thread::sleep_for((interval - elapsed) * 0.95);
          }
        }
        tx.result_->local_latency_per_tx_[get_tx_type(query.type)] =
            tx.result_->local_latency_per_tx_[get_tx_type(query.type)] + elapsed.count();
        return;
    }

    static void insert_factory(Param *param, uint32_t f_id) {
      SimpleKey<8> key;
      Factory::CreateKey(f_id, key.ptr());
      HeapObject obj;
      obj.allocate<Factory>();
      Factory& f_tuple = obj.ref();
      f_tuple.f_id = f_id;
      Tuple* tmp = new Tuple();
      tmp->init(0, TupleBody(key.view(), std::move(obj)), param);
      Masstrees[get_storage(Storage::Factory)].insert_value(key.view(), tmp);
    }

    static void insert_item_master(Param *param, uint32_t i_id) {
      SimpleKey<8> key;
      ItemMaster::CreateKey(i_id, key.ptr());
      HeapObject obj;
      obj.allocate<ItemMaster>();
      ItemMaster& i_tuple = obj.ref();
      i_tuple.i_id = i_id;
      Tuple* tmp = new Tuple();
      tmp->init(0, TupleBody(key.view(), std::move(obj)), param);
      Masstrees[get_storage(Storage::ItemMaster)].insert_value(key.view(), tmp);
    }

    static void insert_item_construction_master([[maybe_unused]] size_t thid, Param *param,
                                                uint32_t parent_id, uint32_t id,
                                                double material_quantity) {
      SimpleKey<8> key;
      ItemConstructionMaster::CreateKey(parent_id, id, key.ptr());
      HeapObject obj;
      obj.allocate<ItemConstructionMaster>();
      ItemConstructionMaster& ic_tuple = obj.ref();
      ic_tuple.ic_parent_i_id = parent_id;
      ic_tuple.ic_i_id = id;
      ic_tuple.ic_material_quantity = material_quantity;
      Tuple* tmp = new Tuple();
      tmp->init(thid, TupleBody(key.view(), std::move(obj)), param);
      Masstrees[get_storage(Storage::ItemConstructionMaster)].insert_value(key.view(), tmp);
    }

    static void insert_item_manufacturing_master([[maybe_unused]] size_t thid, Param *param,
                                                 uint32_t factory_id, uint32_t product_id,
                                                 double quantity) {
      SimpleKey<8> key;
      ItemManufacturingMaster::CreateKey(factory_id, product_id, key.ptr());
      HeapObject obj;
      obj.allocate<ItemManufacturingMaster>();
      ItemManufacturingMaster& im_tuple = obj.ref();
      im_tuple.im_factory_id = factory_id;
      im_tuple.im_product_id = product_id;
      im_tuple.im_quantity = quantity;
      Tuple* tmp = new Tuple();
      tmp->init(thid, TupleBody(key.view(), std::move(obj)), param);
      Masstrees[get_storage(Storage::ItemManufacturingMaster)].insert_value(key.view(), tmp);
    }

    static void insert_material_cost_master([[maybe_unused]] size_t thid, Param *p,
                                            uint32_t f_id, uint32_t m_id, double quantity, double price) {
      SimpleKey<8> key;
      MaterialCostMaster::CreateKey(f_id, m_id, key.ptr());
      HeapObject obj;
      obj.allocate<MaterialCostMaster>();
      MaterialCostMaster& m_tuple = obj.ref();
      m_tuple.mc_f_id = f_id;
      m_tuple.mc_i_id = m_id;
      m_tuple.mc_stock_quantity = quantity;
      m_tuple.mc_stock_price = price;
      Tuple* tmp = new Tuple();
      tmp->init(thid, TupleBody(key.view(), std::move(obj)), p);
      Masstrees[get_storage(Storage::MaterialCostMaster)].insert_value(key.view(), tmp);
    }

    static void insert_product_cost_master([[maybe_unused]] size_t thid, Param *p,
                                         uint32_t f_id, uint32_t p_id, double cost) {
      SimpleKey<8> key;
      ProductCostMaster::CreateKey(f_id, p_id, key.ptr());
      HeapObject obj;
      obj.allocate<ProductCostMaster>();
      ProductCostMaster& p_tuple = obj.ref();
      p_tuple.pc_factory_id = f_id;
      p_tuple.pc_product_id = p_id;
      p_tuple.pc_cost = cost;
      Tuple* tmp = new Tuple();
      tmp->init(thid, TupleBody(key.view(), std::move(obj)), p);
      Masstrees[get_storage(Storage::ProductCostMaster)].insert_value(key.view(), tmp);
    }

    static void load_work_in_progress([[maybe_unused]] size_t thid, Param* param,
                                      std::atomic<uint32_t>& i_id, uint64_t start, uint64_t end) {
#if MASSTREE_USE
      MasstreeWrapper<Tuple>::thread_init(thid);
#endif
      Xoroshiro128Plus rand;
      rand.init();
      for (auto i = start; i <= end; ++i) {
        // TODO: randomized tree size (number of WIP nodes in the tree)
        auto tree_size = FLAGS_bomb_base_tree_size;

        // create root
        Node* root = new Node();
        std::vector<Node*> nodes;
        nodes.push_back(root);

        // add work in progress
        while (nodes.size() < tree_size) {
          assert(!nodes.empty());
          Node* node = new Node();
          Node* parent = nodes.at(rand.random_int(0, nodes.size()-1));
          parent->add_child(node);
          nodes.push_back(node);
        }

        // assign i_id for work in progress recursively
        root->i_id_ = get_i_id_work_start() + i;
        root->assign_id(i_id);

        // assign raw materials
        for (auto& node : nodes) {
          if (!node->is_leaf()) continue;
          auto material_size = FLAGS_bomb_material_per_wip;
          std::set<uint32_t> s;
          while (s.size() < material_size)
            s.emplace(rand.random_int(get_i_id_material_start(),
                                      get_i_id_material_start() + FLAGS_bomb_material_size - 1));
          for (auto& i_id_material : s)
            node->add_child(new Node(i_id_material));
        }

        // insert item construction master
        nodes.clear();
        root->get_all_nodes(nodes);
        for (auto& node : nodes) {
          if (node->parent_ == nullptr) continue;
          insert_item_construction_master(thid, param, node->parent_->i_id_, node->i_id_, 1.0);
        }
      }
    }

    static void load_item_construction_master_product([[maybe_unused]] size_t thid, Param *param,
                                                      uint32_t p_id, std::set<uint32_t>& wip_ids) {
      for (auto& id : wip_ids) {
          insert_item_construction_master(thid, param, p_id, id, 1.0);
      }
    }

    static void load_item_manufacturing_master([[maybe_unused]] size_t thid, Param *param,
                                               Xoroshiro128Plus& rand,
                                               std::set<uint32_t>& product_ids,
                                               std::vector<std::tuple<uint32_t,uint32_t>>& pm_keys) {
      for (uint32_t i = 0; i < FLAGS_bomb_base_product_size_per_factory; i++) { // all
        auto itr = select_random(rand, product_ids);
        for (uint32_t f_id = 1; f_id <= FLAGS_bomb_factory_size; f_id++) {
          uint32_t p_id = *itr;
          insert_item_manufacturing_master(0, param, f_id, *itr, 1.0);
          pm_keys.emplace_back(f_id, *itr);
        }
        product_ids.erase(itr);
      }

      // Disable different product size per factory to improve fairness
      // when choosing a factory in long batch transaction
#ifdef USE_DIFFERENT_PRODUCT_SIZE_PER_FACTORY
      for (uint32_t i = 0; i < 20; i++) { // 50%
        auto itr = select_random(rand, product_ids);
        for (uint32_t f_id = 1; f_id <= FLAGS_bomb_factory_size; f_id++) {
          if (f_id % 2 == 1) {
            insert_item_manufacturing_master(0, param, f_id, *itr, 1.0);
            pm_keys.emplace_back(f_id, *itr);
          }
        }
        product_ids.erase(itr);
      }

      for (uint32_t i = 0; i < 30; i++) { // 25%
        auto itr = select_random(rand, product_ids);
        for (uint32_t f_id = 1; f_id <= FLAGS_bomb_factory_size; f_id++) {
          if (f_id % 4 == 1) {
            insert_item_manufacturing_master(0, param, f_id, *itr, 1.0);
            pm_keys.emplace_back(f_id, *itr);
          }
        }
        product_ids.erase(itr);
      }

      for (uint32_t i = 0; i < 40; i++) { // 10%
        auto itr = select_random(rand, product_ids);
        for (uint32_t f_id = 1; f_id <= FLAGS_bomb_factory_size; f_id++) {
          if (f_id % 10 == 1) {
            insert_item_manufacturing_master(0, param, f_id, *itr, 1.0);
            pm_keys.emplace_back(f_id, *itr);
          }
        }
        product_ids.erase(itr);
      }
#endif
    }

    static void load_material_cost_master([[maybe_unused]] size_t thid, Param *param) {
      for (uint32_t f_id = 1; f_id <= FLAGS_bomb_factory_size; f_id++) {
        for (uint32_t m_id = get_i_id_material_start();
             m_id < get_i_id_material_start() + FLAGS_bomb_material_size; m_id++) {
          insert_material_cost_master(0, param, f_id, m_id, 1.0, 1.0);
        }
      }
    }

    static void load_product_cost_master([[maybe_unused]] size_t thid, Param *param,
                                          std::vector<std::tuple<uint32_t,uint32_t>>& pm_keys) {
      for (auto& [f_id, p_id] : pm_keys) {
        insert_product_cost_master(0, param, f_id, p_id, 0.0);
      }
    }

    static uint32_t getTableNum() {
      return (uint32_t)Storage::Size;
    }

    static void makeDB(Param* param) {
      Xoroshiro128Plus rand;
      rand.init();
      size_t maxthread = 64;
      std::vector<std::thread> thv;
      auto product_start = get_i_id_product_start();
      auto root_wip_start = get_i_id_work_start();
      auto non_root_wip_start = get_i_id_work_start() + FLAGS_bomb_work_size;
      ItemIdCounter.store(non_root_wip_start);

      // TODO: move this codes to appropriate place
      set_tx_name(TxType::UpdateMaterialCostMaster, "UpdateMaterialCostMaster");
      set_tx_name(TxType::UpdateProductCostMaster, "UpdateProductCostMaster");
      set_tx_name(TxType::IssueJournalVoucher, "IssueJournalVoucher");
      set_tx_name(TxType::AddNewProduct, "AddNewProduct");
      set_tx_name(TxType::ChangeRawMaterial, "ChangeRawMaterial");
      set_tx_name(TxType::ChangeProductQuantity, "ChangeProductQuantity");

      std::cout << "load factory" << std::endl;
      for (int f_id = 1; f_id <= FLAGS_bomb_factory_size; f_id++) {
        insert_factory(param, f_id);
      }
 
      // TODO: can we remove thid from loader function?
      // load item construction master for work in progress
      std::cout << "load item construction master for work in progress" << std::endl;
      for (size_t i = 0; i < maxthread; i++) {
        thv.emplace_back(load_work_in_progress, i, param, std::ref(ItemIdCounter),
                        i * (FLAGS_bomb_work_size / maxthread),
                        (i + 1) * ((FLAGS_bomb_work_size + maxthread - 1) / maxthread) - 1);
      }
      for (auto &th : thv) th.join();

      std::cout << "load item master" << std::endl;
      for (int i_id = 1; i_id <= ItemIdCounter; i_id++) {
        insert_item_master(param, i_id);
      }

      // load item construction master for finished products
      std::cout << "load item construction master for finished products" << std::endl;
      std::set<uint32_t> product_id_set; // for loading item manufacturing master
      for (uint32_t p_id = product_start;
           p_id < product_start + FLAGS_bomb_product_size; p_id++) {
        auto tree_size = FLAGS_bomb_tree_num_per_product;
        std::set<uint32_t> s;
        while (s.size() < tree_size)
          s.emplace(rand.random_int(root_wip_start,
                                    root_wip_start + FLAGS_bomb_work_size - 1));
        load_item_construction_master_product(0, param, p_id, s);
        product_id_set.emplace(p_id);
      }

      // load item manufacturing master
      std::cout << "load item manufacturing master " << std::endl;
      std::vector<std::tuple<uint32_t,uint32_t>> pm_keys; // for load product cost master
      load_item_manufacturing_master(0, param, rand, product_id_set, pm_keys);

      // load material cost master
      std::cout << "load material cost master " << std::endl;
      load_material_cost_master(0, param);

      // load product cost master
      std::cout << "load product cost master " << std::endl;
      load_product_cost_master(0, param, pm_keys);

      std::cout << "loading done" << std::endl;
    }

    static TxType decideType(Xoroshiro128Plus& r, uint64_t* thresholds) {
      uint64_t x = r.random_int(1, 100);
      if (x > thresholds[0]) return TxType::UpdateMaterialCostMaster;
      if (x > thresholds[1]) return TxType::IssueJournalVoucher;
      if (x > thresholds[2]) return TxType::AddNewProduct;
      if (x > thresholds[3]) return TxType::ChangeRawMaterial;
      return TxType::ChangeProductQuantity;
    }

    static void request_dispatcher(size_t thid, char &ready, const bool &start, const bool &quit) {
      // prepare queues for short transactions
      int numQueues = TotalThreadNum - FLAGS_bomb_l1_thread_num;
      requestQueues = new ConcurrentQueue<std::pair<TxType, timepoint>>[numQueues];

      uint64_t thresholds[4];
      int32_t perc_s5 = 100 - (FLAGS_bomb_perc_s1 + FLAGS_bomb_perc_s2 + FLAGS_bomb_perc_s3 + FLAGS_bomb_perc_s4);
      if (perc_s5 < 0) {
        std::cout << "Specify short transaction percentage to make the total 100" << std::endl;
        ERR;
      }
      thresholds[3] = perc_s5;
      thresholds[2] = thresholds[3] + FLAGS_bomb_perc_s4;
      thresholds[1] = thresholds[2] + FLAGS_bomb_perc_s3;
      thresholds[0] = thresholds[1] + FLAGS_bomb_perc_s2;

      Xoroshiro128Plus r;
      storeRelease(ready, 1);
      while (!loadAcquire(start)) _mm_pause();
      while (!loadAcquire(quit)) {
        for (int i = 0; i < numQueues; i++) {
          auto start = std::chrono::high_resolution_clock::now();
          for (int j = 0; j < FLAGS_bomb_req_batch_size; j++) {
            requestQueues[i].push(make_pair(decideType(r, thresholds), start));
          }
          if (FLAGS_bomb_mixed_short_rate_tps) {
            std::this_thread::sleep_for(get_request_interval_nano());
          } else {
            std::this_thread::sleep_for(get_request_interval_micro());
          }
        }
      }
      sleepMs(1000);
      delete[] requestQueues;
    }

    static void displayWorkloadParameter() {
    }

    static void displayWorkloadResult() {
    }
};
