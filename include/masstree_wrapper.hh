#pragma once

#include <iomanip>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

#include <pthread.h>
#include <stdlib.h>
#include <xmmintrin.h>

// フォーマッターを利用すると，辞書順のために下記2行が入れ替わる．
// しかし，依存関係があるため，config -> compiler の順にしなければ
// 大量のエラーが出てしまう． そのため，改行を防ぐため空行を空けている
#include "../third_party/masstree/config.h"

#include "../third_party/masstree/compiler.hh"

#include "../third_party/masstree/kvthread.hh"
#include "../third_party/masstree/masstree.hh"
#include "../third_party/masstree/masstree_insert.hh"
#include "../third_party/masstree/masstree_print.hh"
#include "../third_party/masstree/masstree_remove.hh"
#include "../third_party/masstree/masstree_scan.hh"
#include "../third_party/masstree/masstree_stats.hh"
#include "../third_party/masstree/masstree_tcursor.hh"
#include "../third_party/masstree/string.hh"

#include "atomic_wrapper.hh"
#include "debug.hh"
#include "random.hh"
#include "status.hh"
#include "util.hh"
#include "status.hh"

class key_unparse_unsigned {
public:
  static int unparse_key(Masstree::key<std::uint64_t> key, char *buf, int buflen) {
    return snprintf(buf, buflen, "%" PRIu64, key.ikey());
  }
};

/* Notice.
 * type of object is T.
 * inserting a pointer of T as value.
 */
template<typename T>
class MasstreeWrapper {
public:
  static constexpr std::uint64_t insert_bound = UINT64_MAX;  // 0xffffff;
  // static constexpr std::uint64_t insert_bound = 0xffffff; //0xffffff;
  struct table_params : public Masstree::nodeparams<15, 15> {
    typedef T *value_type;
    typedef Masstree::value_print<value_type> value_print_type;
    typedef threadinfo threadinfo_type;
    typedef key_unparse_unsigned key_unparse_type;
    static constexpr ssize_t print_max_indent_depth = 12;
  };

  typedef Masstree::Str Str;
  typedef Masstree::basic_table<table_params> table_type;
  typedef Masstree::unlocked_tcursor<table_params> unlocked_cursor_type;
  typedef Masstree::tcursor<table_params> cursor_type;
  typedef Masstree::leaf<table_params> leaf_type;
  typedef Masstree::internode<table_params> internode_type;

  typedef typename table_type::leaf_type node_type;
  typedef typename unlocked_cursor_type::nodeversion_value_type
          nodeversion_value_type;

  struct insert_info_t {
    const node_type* node;
    uint64_t old_version;
    uint64_t new_version;
  };

  static __thread typename table_params::threadinfo_type *ti;

  MasstreeWrapper() { this->table_init(); }

  /**
   * The low level callback interface is as follows:
   *
   * Consider a scan in the range [a, b):
   *   1) on_resp_node() is called at least once per node which
   *      has a responsibility range that overlaps with the scan range
   *   2) invoke() is called per <k, v>-pair such that k is in [a, b)
   *      (but currently unused in our implementation)
   *
   * The order of calling on_resp_node() and invoke() is up to the implementation.
   */
  class ScanCallback {
    public:
    virtual ~ScanCallback() {}

    /**
     * This node lies within the search range (at version v)
     */
    virtual void on_resp_node(const node_type *n, uint64_t version) = 0;

    /**
     * This key/value pair was read from node n @ version
     */
    virtual bool invoke(const std::string_view &k, T v, const node_type *n, uint64_t version) = 0;
  };

  void table_init() {
    // printf("masstree table_init()\n");
    if (ti == nullptr) ti = threadinfo::make(threadinfo::TI_MAIN, -1);
    table_.initialize(*ti);
    key_gen_ = 0;
    stopping = false;
    printing = 0;
  }

  static void thread_init(int thread_id) {
    if (ti == nullptr) ti = threadinfo::make(threadinfo::TI_PROCESS, thread_id);
  }

  void table_print() {
    table_.print(stdout);
    fprintf(stdout, "Stats: %s\n",
            Masstree::json_stats(table_, ti)
                    .unparse(lcdf::Json::indent_depth(1000))
                    .c_str());
  }

  Status insert_value(std::string_view key, T *value, insert_info_t *insert_info = NULL) {
    cursor_type lp(table_, key.data(), key.size());
    bool found = lp.find_insert(*ti);
    // always_assert(!found, "keys should all be unique");
    if (found) {
      // release lock of existing nodes meaning the first arg equals 0
      lp.finish(0, *ti);
      // return
      return Status::WARN_ALREADY_EXISTS;
    }
    lp.value() = value;
    fence();
    if (insert_info) {
      insert_info->node = lp.node();
      insert_info->old_version = lp.previous_full_version_value();
      insert_info->new_version = lp.next_full_version_value(1);
    }
    lp.finish(1, *ti);
    return Status::OK;
  }

  void insert_value(std::uint64_t key, T *value) {
    std::uint64_t key_buf{__builtin_bswap64(key)};
    insert_value({reinterpret_cast<char *>(&key_buf), sizeof(key_buf)}, value); // NOLINT
  }

  Status remove_value(std::string_view key) {
    cursor_type lp(table_, key.data(), key.size());
    bool found = lp.find_locked(*ti);
    if (found) {
      // try finish_remove. If it fails, following processing unlocks nodes.
      lp.finish(-1, *ti);
      return Status::OK;
    }
    // no nodes
    lp.finish(-1, *ti);
    return Status::WARN_NOT_FOUND;
  }

  T *get_value(std::string_view key) {
    unlocked_cursor_type lp(table_, key.data(), key.size());
    bool found = lp.find_unlocked(*ti);
    if (found) {
      return lp.value();
    }
    return nullptr;
  }

  T *get_value(std::uint64_t key) {
    std::uint64_t key_buf{__builtin_bswap64(key)};
    return get_value({reinterpret_cast<char *>(&key_buf), sizeof(key_buf)});
  }

  void scan(const char *const lkey, const std::size_t len_lkey,
            const bool l_exclusive, const char *const rkey,
            const std::size_t len_rkey, const bool r_exclusive,
            std::vector<T*> *res, int64_t max_scan_num,
            ScanCallback& callback) {
    Str mtkey;
    if (lkey == nullptr) {
      mtkey = Str();
    } else {
      mtkey = Str(lkey, len_lkey);
    }

    SearchRangeScanner scanner(rkey, len_rkey, r_exclusive, res, max_scan_num, callback);
    table_.scan(mtkey, !l_exclusive, scanner, *ti);
  }

  void scan(const char *const lkey, const std::size_t len_lkey,
            const bool l_exclusive, const char *const rkey,
            const std::size_t len_rkey, const bool r_exclusive,
            std::vector<T*> *res, bool limited_scan, ScanCallback& callback) {
    scan(lkey, len_lkey, l_exclusive, rkey, len_rkey, r_exclusive, res,
         limited_scan ? (int64_t) 1000 : (int64_t) -1, callback);
  }

  void scan(const char *const lkey, const std::size_t len_lkey,
            const bool l_exclusive, const char *const rkey,
            const std::size_t len_rkey, const bool r_exclusive,
            std::vector<T*> *res, int64_t max_scan_num) {
    scan(lkey, len_lkey, l_exclusive, rkey, len_rkey, r_exclusive, res, max_scan_num, default_callback);
  }

  // for compatibility
  void scan(const char *const lkey, const std::size_t len_lkey,
            const bool l_exclusive, const char *const rkey,
            const std::size_t len_rkey, const bool r_exclusive,
            std::vector<T*> *res, bool limited_scan) {
    scan(lkey, len_lkey, l_exclusive, rkey, len_rkey, r_exclusive, res, limited_scan, default_callback);
  }

  static inline uint64_t ExtractVersionNumber(const node_type* n) {
    return n->full_version_value();
  }

  static inline std::atomic<bool> stopping{};
  static inline std::atomic<std::uint32_t> printing{};

  class SearchRangeScanner;
  class DefaultScanCallback;
  static inline DefaultScanCallback default_callback{};

private:
  table_type table_;
  std::uint64_t key_gen_;

  static inline Str make_key(std::uint64_t int_key, std::uint64_t &key_buf) {
    key_buf = __builtin_bswap64(int_key);
    return Str((const char *) &key_buf, sizeof(key_buf));
  }
};

template<typename T>
class MasstreeWrapper<T>::DefaultScanCallback : public ScanCallback {
  void on_resp_node(const node_type *n, uint64_t version) {}
  bool invoke(const std::string_view &k, T v, const node_type *n, uint64_t version) {
    return true;
  }
};

template<typename T>
class MasstreeWrapper<T>::SearchRangeScanner {
  public:
  using Str = Masstree::Str;

  SearchRangeScanner(const char *const rkey, const std::size_t len_rkey,
                     const bool r_exclusive, std::vector<T *> *scan_buffer,
                     int64_t max_scan_num, ScanCallback& callback)
      : rkey_(rkey),
        len_rkey_(len_rkey),
        r_exclusive_(r_exclusive),
        scan_buffer_(scan_buffer),
        max_scan_num_(max_scan_num),
        callback_(callback) {
    if (max_scan_num_ > 0) {
      scan_buffer->reserve(max_scan_num_);
    }
  }

  void visit_leaf(const Masstree::scanstackelt<table_params>& iter,
                  const Masstree::key<uint64_t>& key,
                  threadinfo& ti) {
    const Masstree::leaf<table_params>* node = iter.node();
    uint64_t version = iter.full_version_value();
    callback_.on_resp_node(node, version);
  }

  bool visit_value(const Str key, T *val, threadinfo &) {
    if (max_scan_num_ >= 0 && scan_buffer_->size() >= max_scan_num_) {
      return false;
    }

    if (rkey_ == nullptr) {
      scan_buffer_->emplace_back(val);
      return true;
    }

    const int res_memcmp = memcmp(
        rkey_, key.s, std::min(len_rkey_, static_cast<std::size_t>(key.len)));
    if (res_memcmp > 0 ||
        (res_memcmp == 0 &&
         ((!r_exclusive_ && len_rkey_ == static_cast<std::size_t>(key.len)) ||
          len_rkey_ > static_cast<std::size_t>(key.len)))) {
      scan_buffer_->emplace_back(val);
      return true;
    }
    return false;
  }

  private:
  const char *const rkey_{};
  const std::size_t len_rkey_{};
  const bool r_exclusive_{};
  std::vector<T *> *scan_buffer_{};
  int64_t max_scan_num_ = -1;
  ScanCallback& callback_;
};

template<typename T>
__thread typename MasstreeWrapper<T>::table_params::threadinfo_type *
        MasstreeWrapper<T>::ti = nullptr;
#ifdef GLOBAL_VALUE_DEFINE
volatile mrcu_epoch_type active_epoch = 1;
volatile std::uint64_t globalepoch = 1;
volatile bool recovering = false;
#else
extern volatile mrcu_epoch_type active_epoch;
extern volatile std::uint64_t globalepoch;
extern volatile bool recovering;
#endif
