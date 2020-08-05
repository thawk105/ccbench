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
#include "util.hh"

class key_unparse_unsigned {
public:
  static int unparse_key(Masstree::key<std::uint64_t> key, char *buf, int buflen) {
    return snprintf(buf, buflen, "%"
                                 PRIu64, key.ikey());
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

  typedef typename table_type::node_type node_type;
  typedef typename unlocked_cursor_type::nodeversion_value_type
          nodeversion_value_type;

  static __thread typename table_params::threadinfo_type *ti;

  MasstreeWrapper() { this->table_init(); }

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

  void insert_value(std::string_view key, T *value) {
    cursor_type lp(table_, key.data(), key.size());
    bool found = lp.find_insert(*ti);
    // always_assert(!found, "keys should all be unique");
    if (found) {
      // release lock of existing nodes meaning the first arg equals 0
      lp.finish(0, *ti);
      // return
      return;
    }
    lp.value() = value;
    fence();
    lp.finish(1, *ti);
    return;
  }

  void insert_value(std::uint64_t key, T *value) {
    std::uint64_t key_buf{__builtin_bswap64(key)};
    insert_value({reinterpret_cast<char *>(&key_buf), sizeof(key_buf)}, value); // NOLINT
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

  static inline std::atomic<bool> stopping{};
  static inline std::atomic<std::uint32_t> printing{};

private:
  table_type table_;
  std::uint64_t key_gen_;

  static inline Str make_key(std::uint64_t int_key, std::uint64_t &key_buf) {
    key_buf = __builtin_bswap64(int_key);
    return Str((const char *) &key_buf, sizeof(key_buf));
  }
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
