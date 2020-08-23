#pragma once

#include <pthread.h>
#include <xmmintrin.h>

#include <atomic>
#include <array>
#include <cstdlib>
#include <random>
#include <thread>
#include <typeinfo>
#include <vector>

#include "record.h"
#include "scheme_global.h"
#include "tpcc_tables.hpp"

/* if you use formatter, following 2 lines may be exchange.
 * but there is a dependency relation, so teh order is restricted config ->
 * compiler. To depend exchanging 2 lines, insert empty line. */
// config
#include "masstree/config.h"
// compiler
#include "masstree/compiler.hh"

// masstree-header
#include "masstree/kvthread.hh"
#include "masstree/masstree.hh"
#include "masstree/masstree_insert.hh"
#include "masstree/masstree_print.hh"
#include "masstree/masstree_remove.hh"
#include "masstree/masstree_scan.hh"
#include "masstree/masstree_stats.hh"
#include "masstree/masstree_tcursor.hh"
#include "masstree/string.hh"

namespace ccbench {

class key_unparse_unsigned {  // NOLINT
public:
  [[maybe_unused]] static int unparse_key(  // NOLINT
          Masstree::key<uint64_t> key, char *buf, int buflen) {
    return snprintf(buf, buflen, "%" PRIu64, key.ikey());  // NOLINT
  }
};

template<typename T>
class SearchRangeScanner {
public:
  using Str = Masstree::Str;

  SearchRangeScanner(const char *const rkey, const std::size_t len_rkey,
                     const bool r_exclusive, std::vector<const T *> *scan_buffer,
                     bool limited_scan)
          : rkey_(rkey),
            len_rkey_(len_rkey),
            r_exclusive_(r_exclusive),
            scan_buffer_(scan_buffer),
            limited_scan_(limited_scan) {
    if (limited_scan) {
      scan_buffer->reserve(kLimit_);
    }
  }

  template<typename SS, typename K>
  [[maybe_unused]] void visit_leaf(const SS &, const K &, threadinfo &) {}

  [[maybe_unused]] bool visit_value(const Str key, T *val,  // NOLINT
                                    threadinfo &) {
    if (limited_scan_) {
      if (scan_buffer_->size() >= kLimit_) {
        return false;
      }
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
  std::vector<const T *> *scan_buffer_{};
  const bool limited_scan_{false};
  static constexpr std::size_t kLimit_ = 1000;
};

/* Notice.
 * type of object is T.
 * inserting a pointer of T as value.
 */
template<typename T>
class masstree_wrapper {  // NOLINT
public:
  [[maybe_unused]] static constexpr uint64_t insert_bound =
          UINT64_MAX;  // 0xffffff;
  // static constexpr uint64_t insert_bound = 0xffffff; //0xffffff;
  struct table_params : public Masstree::nodeparams<15, 15> {  // NOLINT
    using value_type = T *;
    using value_print_type = Masstree::value_print<value_type>;
    using threadinfo_type = threadinfo;
    using key_unparse_type = key_unparse_unsigned;
    [[maybe_unused]] static constexpr ssize_t print_max_indent_depth = 12;
  };

  using Str = Masstree::Str;
  using table_type = Masstree::basic_table<table_params>;
  using unlocked_cursor_type = Masstree::unlocked_tcursor<table_params>;
  using cursor_type = Masstree::tcursor<table_params>;
  using leaf_type = Masstree::leaf<table_params>;
  using internode_type = Masstree::internode<table_params>;

  using node_type = typename table_type::node_type;
  using nodeversion_value_type =
  typename unlocked_cursor_type::nodeversion_value_type;

  // tanabe :: todo. it should be wrapped by atomic type if not only one thread
  // use this.
  static __thread typename table_params::threadinfo_type *ti;

  masstree_wrapper() { this->table_init(); }

  ~masstree_wrapper() = default;

  void table_init() {
    // printf("masstree table_init()\n");
    if (ti == nullptr) ti = threadinfo::make(threadinfo::TI_MAIN, -1);
    table_.initialize(*ti);
    stopping = false;
    printing = 0;
  }

  static void thread_init(int thread_id) {
    if (ti == nullptr) ti = threadinfo::make(threadinfo::TI_PROCESS, thread_id);
  }

  /**
   * @brief insert value to masstree
   * @param key This must be a type of const char*.
   * @param len_key
   * @param value
   * @details future work, we try to delete making temporary
   * object std::string buf(key). But now, if we try to do
   * without making temporary object, it fails by masstree.
   * @return Status::WARN_ALREADY_EXISTS The records whose key is the same as @a
   * key exists in masstree, so this function returned immediately.
   * @return Status::OK success.
   */
  Status insert_value(const char *key,      // NOLINT
                      std::size_t len_key,  // NOLINT
                      T *value) {
    cursor_type lp(table_, key, len_key);
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
    lp.finish(1, *ti);
    return Status::OK;
  }

  Status insert_value(std::string_view key, T *value) {
    return insert_value(key.data(), key.size(), value);
  }

  // for bench.
  Status put_value(const char *key, std::size_t len_key,  // NOLINT
                   T *value, T **record) {
    cursor_type lp(table_, key, len_key);
    bool found = lp.find_locked(*ti);
    if (found) {
      *record = lp.value();
      *value = **record;
      lp.value() = value;
      fence();
      lp.finish(0, *ti);
      return Status::OK;
    }
    fence();
    lp.finish(0, *ti);
    /**
     * Look project_root/third_party/masstree/masstree_get.hh:98 and 100.
     * If the node wasn't found, the lock was acquired and tcursor::state_ is
     * 0. So it needs to release. If state_ == 0, finish function merely
     * release locks of existing nodes.
     */
    return Status::WARN_NOT_FOUND;
  }

  Status remove_value(const char *key,        // NOLINT
                      std::size_t len_key) {  // NOLINT
    cursor_type lp(table_, key, len_key);
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

  T *get_value(const char *key, std::size_t len_key) {  // NOLINT
    unlocked_cursor_type lp(table_, key, len_key);
    bool found = lp.find_unlocked(*ti);
    if (found) {
      return lp.value();
    }
    return nullptr;
  }

  T *get_value(std::string_view key) {
    return get_value(key.data(), key.size());
  }

  void scan(const char *const lkey, const std::size_t len_lkey,
            const bool l_exclusive, const char *const rkey,
            const std::size_t len_rkey, const bool r_exclusive,
            std::vector<const T *> *res, bool limited_scan) {
    Str mtkey;
    if (lkey == nullptr) {
      mtkey = Str();
    } else {
      mtkey = Str(lkey, len_lkey);
    }

    SearchRangeScanner<T> scanner(rkey, len_rkey, r_exclusive, res,
                                  limited_scan);
    table_.scan(mtkey, !l_exclusive, scanner, *ti);
  }

  [[maybe_unused]] void print_table() {
    // future work.
  }

  [[maybe_unused]] static inline std::atomic<bool> stopping{};      // NOLINT
  [[maybe_unused]] static inline std::atomic<uint32_t> printing{};  // NOLINT

private:
  table_type table_;

  [[maybe_unused]] static inline Str make_key(std::string &buf) {  // NOLINT
    return Str(buf);
  }
};

template<typename T>
__thread typename masstree_wrapper<T>::table_params::threadinfo_type *
        masstree_wrapper<T>::ti = nullptr;

[[maybe_unused]] extern volatile bool recovering;

class kohler_masstree {
public:
  static constexpr std::size_t db_length = 10;

  /**
   * @brief find record from masstree by using args informations.
   * @return the found record pointer.
   */
  static void *find_record(Storage st, std::string_view key);

  static masstree_wrapper<Record> &get_mtdb(Storage st) {
    return MTDB.at(static_cast<std::uint32_t>(st));
  }

  /**
   * @brief insert record to masstree by using args informations.
   * @pre the record which has the same key as the key of args have never been
   * inserted.
   * @param key
   * @param record It inserts this pointer to masstree database.
   * @return WARN_ALREADY_EXISTS The records whose key is the same as @a key
   * exists in masstree, so this function returned immediately.
   * @return Status::OK It inserted record.
   */
  static Status insert_record(Storage st, std::string_view key, Record *record); // NOLINT

private:
  static inline std::array<masstree_wrapper<Record>, db_length> MTDB;  // NOLINT
};

}  // namespace ccbench
