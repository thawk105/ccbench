#pragma once

#include <iomanip>
#include <iostream>
#include <random>
#include <vector>
#include <thread>

#include <pthread.h>
#include <stdlib.h>
#include <xmmintrin.h>

#include "../masstree/config.h"
#include "../masstree/compiler.hh"

#include "../masstree/masstree.hh"
#include "../masstree/kvthread.hh"
#include "../masstree/masstree_tcursor.hh"
#include "../masstree/masstree_insert.hh"
#include "../masstree/masstree_print.hh"
#include "../masstree/masstree_remove.hh"
#include "../masstree/masstree_scan.hh"
#include "../masstree/masstree_stats.hh"
#include "../masstree/string.hh"

#include "atomic_wrapper.hh"
#include "debug.hh"
#include "util.hh"
#include "random.hh"

class key_unparse_unsigned {
public:
  static int unparse_key(Masstree::key<uint64_t> key, char* buf, int buflen) {
    return snprintf(buf, buflen, "%" PRIu64, key.ikey());
  }
};

/* Notice.
 * type of object is T.
 * inserting a pointer of T as value.
 */
template <typename T>
class MasstreeWrapper {
public:
  static constexpr uint64_t insert_bound = UINT64_MAX; //0xffffff;
  //static constexpr uint64_t insert_bound = 0xffffff; //0xffffff;
  struct table_params : public Masstree::nodeparams<15,15> {
    typedef T* value_type;
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
  typedef typename unlocked_cursor_type::nodeversion_value_type nodeversion_value_type;

  static __thread typename table_params::threadinfo_type *ti;

  MasstreeWrapper() {
    this->table_init();
  }

  void table_init() {
    //printf("masstree table_init()\n");
    if (ti == nullptr)
      ti = threadinfo::make(threadinfo::TI_MAIN, -1);
    table_.initialize(*ti);
    key_gen_ = 0;
    stopping = false;
    printing = 0;
  }

  static void thread_init(int thread_id) {
    if (ti == nullptr)
      ti = threadinfo::make(threadinfo::TI_PROCESS, thread_id);
  }

  void table_print()
  {
    table_.print(stdout);
    fprintf(stdout, "Stats: %s\n",
      Masstree::json_stats(table_, ti).unparse(lcdf::Json::indent_depth(1000)).c_str());
  }

  void insert_value(uint64_t keyid, T* value) {
    Str key;
    uint64_t key_buf;

    key = make_key(keyid, key_buf);
    cursor_type lp(table_, key);
    bool found = lp.find_insert(*ti);
    //if (!found) ERR;
    always_assert(!found, "keys should all be unique");
    lp.value() = value;
    fence();
    lp.finish(1, *ti);
  }

  T* get_value(uint64_t keyid) {
    Str key;
    uint64_t key_buf;
    key = make_key(keyid, key_buf);
    unlocked_cursor_type lp(table_, key);
    bool found = lp.find_unlocked(*ti);
    if (!found) ERR;
    return lp.value();
  }

    static bool stopping;
    static uint32_t printing;
private:
    table_type table_;
    uint64_t key_gen_;

    static inline Str make_key(uint64_t int_key, uint64_t& key_buf) {
        key_buf = __builtin_bswap64(int_key);
        return Str((const char *)&key_buf, sizeof(key_buf));
    }
};

template<typename T>
__thread typename MasstreeWrapper<T>::table_params::threadinfo_type * MasstreeWrapper<T>::ti = nullptr;
template<typename T>
bool MasstreeWrapper<T>::stopping = false;
template<typename T>
uint32_t MasstreeWrapper<T>::printing = 0;
#ifdef GLOBAL_VALUE_DEFINE
volatile mrcu_epoch_type active_epoch = 1;
volatile uint64_t globalepoch = 1;
volatile bool recovering = false;
#else
extern volatile mrcu_epoch_type active_epoch;
extern volatile uint64_t globalepoch;
extern volatile bool recovering;
#endif
