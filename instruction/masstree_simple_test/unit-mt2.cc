#include <iomanip>
#include <iostream>
#include <random>
#include <vector>
#include <thread>

#include <pthread.h>
#include <stdlib.h>
#include <xmmintrin.h>

#include "../../masstree/config.h"
#include "../../masstree/compiler.hh"

#include "../../masstree/masstree.hh"
#include "../../masstree/kvthread.hh"
#include "../../masstree/masstree_tcursor.hh"
#include "../../masstree/masstree_insert.hh"
#include "../../masstree/masstree_print.hh"
#include "../../masstree/masstree_remove.hh"
#include "../../masstree/masstree_scan.hh"
#include "../../masstree/masstree_stats.hh"
#include "../../masstree/string.hh"

#include "../../include/atomic_wrapper.hpp"
#include "../../include/debug.hpp"
#include "../../include/util.hpp"
#include "../../include/random.hpp"

extern bool isReady(const std::vector<char> &readys);

extern void waitForReady(const std::vector<char> &readys);

extern void sleepMs(size_t ms);

size_t NUM_THREADS = 0;
#define EX_TIME 3

using std::cout;
using std::endl;

class key_unparse_unsigned {
public:
  static int unparse_key(Masstree::key <uint64_t> key, char *buf, int buflen) {
    return snprintf(buf, buflen, "%"
    PRIu64, key.ikey());
  }
};

class MasstreeWrapper {
public:
  struct Myrecord {
    uint64_t *val;
  };

  static constexpr uint64_t insert_bound = UINT64_MAX; //0xffffff;
  //static constexpr uint64_t insert_bound = 0xffffff; //0xffffff;
  struct table_params : public Masstree::nodeparams<15, 15> {
    typedef Myrecord *value_type;
    typedef Masstree::value_print <value_type> value_print_type;
    typedef threadinfo threadinfo_type;
    typedef key_unparse_unsigned key_unparse_type;
    static constexpr ssize_t print_max_indent_depth = 12;
  };

  typedef Masstree::Str Str;
  typedef Masstree::basic_table <table_params> table_type;
  typedef Masstree::unlocked_tcursor <table_params> unlocked_cursor_type;
  typedef Masstree::tcursor <table_params> cursor_type;
  typedef Masstree::leaf <table_params> leaf_type;
  typedef Masstree::internode <table_params> internode_type;

  typedef typename table_type::node_type node_type;
  typedef typename unlocked_cursor_type::nodeversion_value_type nodeversion_value_type;

  static __thread typename table_params::threadinfo_type *ti;

  MasstreeWrapper() {
    this->table_init();
  }

  void table_init() {
    if (ti == nullptr)
      ti = threadinfo::make(threadinfo::TI_MAIN, -1);
    table_.initialize(*ti);
    key_gen_ = 0;
  }

  void keygen_reset() {
    key_gen_ = 0;
  }

  static void thread_init(int thread_id) {
    if (ti == nullptr)
      ti = threadinfo::make(threadinfo::TI_PROCESS, thread_id);
  }

  void table_print() {
    //table_.print(stdout);
    fprintf(stdout, "Stats: %s\n",
            Masstree::json_stats(table_, ti).unparse(lcdf::Json::indent_depth(1000)).c_str());
  }

  table_type table_;
private:
  uint64_t key_gen_;
  static bool stopping;
  static uint32_t printing;

  static inline Str make_key(uint64_t int_key, uint64_t &key_buf) {
    key_buf = __builtin_bswap64(int_key);
    return Str((const char *) &key_buf, sizeof(key_buf));
  }
};

__thread typename MasstreeWrapper::table_params::threadinfo_type *MasstreeWrapper::ti = nullptr;
bool MasstreeWrapper::stopping = false;
uint32_t MasstreeWrapper::printing = 0;

volatile mrcu_epoch_type active_epoch = 1;
volatile uint64_t globalepoch = 1;
volatile bool recovering = false;

int
main() {
  auto mt = new MasstreeWrapper();
  mt->keygen_reset();

  uint64_t valob = 39;
  MasstreeWrapper::Myrecord record;
  cout << "&record\t" << &record << endl;

  record.val = &valob;

  Masstree::Str key;
  Masstree::tcursor <MasstreeWrapper::table_params> lp(mt->table_, key);
  bool found = lp.find_insert(*(mt->ti));
  always_assert(!found, "this key already appeared.");

  lp.value() = &record;
  fence();

  printf("%lu\n", *((lp.value())->val));
  cout << "&(lp.value())\t" << lp.value() << endl;
  lp.finish(1, *(mt->ti));

  return 0;
}
