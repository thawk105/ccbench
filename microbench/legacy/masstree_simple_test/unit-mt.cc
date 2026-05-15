
#include <cmath>
#include <cfloat>
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

#ifdef dbs11
#include <concurrent_hash_map.h>

size_t hashsize = pow(2, 30) - 1;
struct MyHashCompare {
  static size_t hash(const uint64_t& x) {
    return x & (hashsize);
  }
  static bool equal(const uint64_t& a, const uint64_t& b) {
    return a==b;
  }
};
typedef tbb::concurrent_hash_map<uint64_t, uint64_t, MyHashCompare> UintHashTable;
UintHashTable HashTable;
#endif

extern bool isReady(const std::vector<char> &readys);

extern void waitForReady(const std::vector<char> &readys);

extern void sleepMs(size_t ms);

size_t NUM_THREADS = 0;
size_t TUPLE_NUM_2exponent = 0;
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
  static constexpr uint64_t insert_bound = UINT64_MAX; //0xffffff;
  //static constexpr uint64_t insert_bound = 0xffffff; //0xffffff;
  struct table_params : public Masstree::nodeparams<15, 15> {
    typedef uint64_t value_type;
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

  void insert_test(size_t thid, char &ready, const bool &start, const bool &quit, uint64_t &count) {
    uint64_t lcount(0);
    uint64_t lkey_gen(thid);

    storeRelease(ready, 1);
    while (!loadAcquire(start)) _mm_pause();
    while (!loadAcquire(quit)) {
#if 0
      auto int_key = fetch_and_add(&key_gen_, 1);
#else
      auto int_key = lkey_gen;
      lkey_gen += NUM_THREADS;
#endif
      uint64_t key_buf;
      if (int_key > insert_bound) {
        printf("int_key > insert_bound\n");
        exit(1);
        break;
      }
      Str key = make_key(int_key, key_buf);

      cursor_type lp(table_, key);
      bool found = lp.find_insert(*ti);
      always_assert(!found, "keys should all be unique");

      lp.value() = int_key;

      fence();
      lp.finish(1, *ti);
      ++lcount;
    }

    storeRelease(count, lcount);
    //printf("Th#%zu:\t%lu\n", thid, count);
  }

  void put_test(size_t thid, char &ready, const bool &start, const bool &quit, uint64_t &count) {
    uint64_t lcount(0);
    Str key;
    Xoroshiro128Plus rand;
    rand.init();
    size_t key_buf;
    size_t mask = pow(2, TUPLE_NUM_2exponent) - 1;

    storeRelease(ready, 1);
    while (!loadAcquire(start)) _mm_pause();
    while (!loadAcquire(quit)) {
      key = make_key(rand() & mask, key_buf);
      cursor_type lp(table_, key);

      bool found = lp.find_locked(*ti);
      if (!found) {
        ERR;
      } else {
        lp.value() = thid;
        ++lcount;
        //printf("%lu\n", lp.value());
      }

      fence();
      lp.finish(0, *ti);
    }

    storeRelease(count, lcount);
    //printf("Th#%zu:\t%lu\n", thid, count);
  }

  void get_test([[maybe_unused]]size_t thid, char &ready, const bool &start, const bool &quit, uint64_t &count) {
    uint64_t lcount(0);
    Str key;
    Xoroshiro128Plus rand;
    rand.init();
    size_t key_buf;
    size_t mask = pow(2, TUPLE_NUM_2exponent) - 1;

    storeRelease(ready, 1);
    while (!loadAcquire(start)) _mm_pause();
    while (!loadAcquire(quit)) {
      key = make_key(rand() & mask, key_buf);
      unlocked_cursor_type lp(table_, key);
      bool found = lp.find_unlocked(*ti);
      if (found) {
        //printf("%lu\n", lp.value());
        ++lcount;
      }
    }

    storeRelease(count, lcount);
    //printf("Th#%zu:\t%lu\n", thid, count);
  }

  void insert_test_hashTable(size_t thid, char &ready, const bool &start, const bool &quit, uint64_t &count) {
    uint64_t lcount(0);
    uint64_t lkey_gen(thid);

    storeRelease(ready, 1);
    while (!loadAcquire(start)) _mm_pause();
    while (!loadAcquire(quit)) {

      auto int_key = lkey_gen;
      lkey_gen += NUM_THREADS;

      UintHashTable::accessor a;
      HashTable.insert(a, int_key);
      ++lcount;
    }

    storeRelease(count, lcount);
    //printf("Th#%zu:\t%lu\n", thid, count);
  }

  void remove_test() {
    while (1) {
      auto int_key = fetch_and_add(&key_gen_, 1);
      uint64_t key_buf;
      if (int_key > insert_bound)
        break;
      Str key = make_key(int_key, key_buf);

      cursor_type lp(table_, key);
      bool found = lp.find_locked(*ti);
      always_assert(found, "keys must all exist");
      lp.finish(-1, *ti);
    }
  }

  void insert_remove_test(int thread_id) {
    std::mt19937 gen(thread_id);
    std::uniform_int_distribution<int> dist(1, 6);
    uint64_t int_key = 0;
    bool need_print = true;
    while (!stopping) {
      int_key = fetch_and_add(&key_gen_, 1);
      uint64_t key_buf;
      if (int_key > insert_bound)
        break;
      Str key = make_key(int_key, key_buf);

      cursor_type lp(table_, key);
      bool found = lp.find_insert(*ti);
      always_assert(!found, "keys should all be unique 1");

      lp.value() = int_key;
      fence();
      lp.finish(1, *ti);

      if (dist(gen) <= 2) {
        cursor_type lp1(table_, key);
        bool found1 = lp1.find_locked(*ti);
        if (!found1) {
          stopping = true;
          lp1.finish(0, *ti);
          printf("failed at key %"
          PRIu64
          ", lp1 got %p\n", int_key, lp1.node());
          need_print = true;
          break;
          always_assert(found1, "this is my key!");
        } else {
          lp1.finish(-1, *ti);
        }
      }
    }
    printf("stopped at key %"
    PRIu64
    "\n", int_key);
    if (need_print && fetch_and_add(&printing, 1) == 0) {
      table_.print(stdout);
      fflush(stdout);
      fprintf(stdout, "Stats: %s\n",
              Masstree::json_stats(table_, ti).unparse(lcdf::Json::indent_depth(1000)).c_str());
    }
  }

  void table_print() {
    //table_.print(stdout);
    fprintf(stdout, "Stats: %s\n",
            Masstree::json_stats(table_, ti).unparse(lcdf::Json::indent_depth(1000)).c_str());
  }

  static inline Str make_key(uint64_t int_key, uint64_t &key_buf) {
    key_buf = __builtin_bswap64(int_key);
    return Str((const char *) &key_buf, sizeof(key_buf));
  }

  table_type table_;
private:
  uint64_t key_gen_;
  static bool stopping;
  static uint32_t printing;
};

__thread typename MasstreeWrapper::table_params::threadinfo_type *MasstreeWrapper::ti = nullptr;
bool MasstreeWrapper::stopping = false;
uint32_t MasstreeWrapper::printing = 0;

volatile mrcu_epoch_type active_epoch = 1;
volatile uint64_t globalepoch = 1;
volatile bool recovering = false;

void test_thread(MasstreeWrapper *mt, size_t thid, char &ready, const bool &start, const bool &quit, uint64_t &count) {
  mt->thread_init(thid);
  //mt->insert_test(thid, ready, start, quit, count);
  //mt->get_test(thid, ready, start, quit, count);
  mt->put_test(thid, ready, start, quit, count);
  //mt->insert_test_hashTable(thid, ready, start, quit, count);
  //mt->insert_remove_test(thread_id);
}

void part_make_tree(MasstreeWrapper *mt, size_t thid, uint64_t start, uint64_t end) {
  mt->thread_init(thid);
  MasstreeWrapper::Str key;
  size_t key_buf;

  for (uint64_t i = start; i <= end; ++i) {
    key = mt->make_key(i, key_buf);

    MasstreeWrapper::cursor_type lp(mt->table_, key);
    bool found = lp.find_insert(*(mt->ti));
    always_assert(!found, "this key already appeared.");

    lp.value() = 0;

    fence();
    lp.finish(1, *(mt->ti));
  }
}

void pre_make_tree(MasstreeWrapper *mt) {
  /* 
   * maxthread は masstree 構築の最大並行スレッド数．
   * 初期値はハードウェア最大値
   * pow(2, TUPLE_NUM_2exponent) を均等に分割できる最大スレッド数を求める．
   */
  //cout << DBL_MAX << endl;
  size_t maxthread = 0;
  for (size_t i = std::thread::hardware_concurrency(); i > 0; --i) {
    if ((size_t) pow(2, TUPLE_NUM_2exponent) % i == 0) {
      maxthread = i;
      break;
    }
    if (i == 1) ERR;
    // 1 thread でも剰余 0 は自明にERR
  }

  std::vector <std::thread> thv;
  for (size_t i = 0; i < maxthread; ++i) {
    thv.emplace_back(part_make_tree, mt, i, i * (pow(2, TUPLE_NUM_2exponent) / maxthread),
                     (i + 1) * (pow(2, TUPLE_NUM_2exponent) / maxthread) - 1);
  }
  for (auto &th : thv) th.join();
}

int
main([[maybe_unused]]int argc, char *argv[]) {
  NUM_THREADS = atoi(argv[1]);
  TUPLE_NUM_2exponent = atoi(argv[2]);
  if (pow(2, TUPLE_NUM_2exponent) == HUGE_VAL) ERR;
  bool start = false;
  bool quit = false;
  std::vector<char> readys(NUM_THREADS);
  std::vector <uint64_t> counts(NUM_THREADS);

  auto mt = new MasstreeWrapper();
  mt->keygen_reset();
  pre_make_tree(mt);

  std::vector <std::thread> ths;

  for (size_t i = 0; i < NUM_THREADS; ++i)
    ths.emplace_back(test_thread, mt, i, std::ref(readys[i]), std::ref(start), std::ref(quit), std::ref(counts[i]));

  waitForReady(readys);
  storeRelease(start, true);
  for (size_t i = 0; i < EX_TIME; ++i) {
    sleepMs(1000);
  }
  storeRelease(quit, true);

  for (auto &t : ths)
    t.join();

  uint64_t sum(0);
  for (size_t i = 0; i < NUM_THREADS; ++i) {
    sum += counts[i];
  }

  //mt->table_print();
  cout << std::fixed << std::setprecision(2) << (double) (sum / EX_TIME) / 1000000.0 << endl;
  return 0;
}
