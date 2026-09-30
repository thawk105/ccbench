#pragma once

#include <thread>
#include <vector>

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
#if TRACE
#include "trace.hh"
#endif
#line 16

#include "gflags/gflags.h"

#ifdef GLOBAL_VALUE_DEFINE
DEFINE_bool(ycsb_rmw, false,
            "True means read modify write, false means blind write.");
DEFINE_uint64(ycsb_max_ope, 10,
              "Total number of operations per single transaction.");
DEFINE_uint64(ycsb_rratio, 50, "read ratio of single transaction.");
DEFINE_uint64(ycsb_tuple_num, 1000000, "Total number of records.");
DEFINE_double(ycsb_zipf_skew, 0, "zipf skew. 0 ~ 0.999...");
#else
DECLARE_bool(ycsb_rmw);
DECLARE_uint64(ycsb_max_ope);
DECLARE_uint64(ycsb_rratio);
DECLARE_uint64(ycsb_tuple_num);
DECLARE_double(ycsb_zipf_skew);
#endif

enum class Storage : std::uint32_t {
  YCSB = 0,
  Size,
};

struct YCSB {
  alignas(CACHE_LINE_SIZE) std::uint64_t id_;
  char val_[VAL_SIZE];

  //Primary Key: key_
  //key size is 8 bytes.
  static void CreateKey(uint64_t id, char* out) {
    assign_as_bigendian(id, &out[0]);
  }

  void createKey(char* out) const { return CreateKey(id_, out); }

  [[nodiscard]] std::string_view view() const { return struct_str_view(*this); }
};

inline static void makeProcedure(std::vector<Procedure>& pro,
                                 Xoroshiro128Plus& rnd, FastZipf& zipf) {
  pro.clear();
  bool ronly_flag(true), wonly_flag(true);
  for (size_t i = 0; i < FLAGS_ycsb_max_ope; ++i) {
    uint64_t tmpkey;
    // decide access destination key.
    tmpkey = zipf() % FLAGS_ycsb_tuple_num;

    // decide operation type.
    if ((rnd.next() % 100) < FLAGS_ycsb_rratio) {
      wonly_flag = false;
      pro.emplace_back(Ope::READ, tmpkey);
    } else {
      ronly_flag = false;
      if (FLAGS_ycsb_rmw) {
        pro.emplace_back(Ope::READ_MODIFY_WRITE, tmpkey);
      } else {
        pro.emplace_back(Ope::WRITE, tmpkey);
      }
    }
  }

  (*pro.begin()).ronly_ = ronly_flag;
  (*pro.begin()).wonly_ = wonly_flag;

#if KEY_SORT
  std::sort(pro.begin(), pro.end());
#endif // KEY_SORT
}

class YcsbWorkload {
public:
  Xoroshiro128Plus rnd_;
  FastZipf zipf_;

  YcsbWorkload() {
    rnd_.init();
    FastZipf zipf(&rnd_, FLAGS_ycsb_zipf_skew, FLAGS_ycsb_tuple_num);
    zipf_ = zipf;
  }

  template <typename TxExecutor, typename TransactionStatus>
  void run(TxExecutor& tx) {
#if ADD_ANALYSIS
    uint64_t start = rdtscp();
#endif
    makeProcedure(tx.pro_set_, rnd_, zipf_);
#if ADD_ANALYSIS
    tx.result_->local_make_procedure_latency_ += rdtscp() - start;
#endif
    tx.is_ronly_ = (*tx.pro_set_.begin()).ronly_;

#if TRACE
    std::vector<izanagi_trace::GateOp> izanagi_steps;
#endif
#line 108
  RETRY:
#if TRACE
    izanagi_steps.clear();
    izanagi_trace::discard_gate_txid();
#endif
#line 109
    if (tx.isLeader()) { tx.leaderWork(); }

    if (loadAcquire(tx.quit_)) return;

    tx.begin();
    SimpleKey<8> key[tx.pro_set_.size()];
    HeapObject obj[tx.pro_set_.size()];
    uint64_t i = 0;
    for (auto& pro : tx.pro_set_) {
      YCSB::CreateKey(pro.key_, key[i].ptr());
      uint64_t k;
      parse_bigendian(key[i].view().data(), k);
      if (pro.ope_ == Ope::READ) {
        TupleBody* body;
        tx.read(Storage::YCSB, key[i].view(), &body);
        if (tx.status_ != TransactionStatus::aborted) {
          // Touch the value so the read is not optimized away.
          [[maybe_unused]] YCSB& t = body->get_value().cast_to<YCSB>();
#if TRACE
          izanagi_steps.push_back(
              {'R', izanagi_trace::key_to_hex(std::string(key[i].view())),
               std::to_string(t.id_), "-"});
#endif
#line 127
        }
      } else if (pro.ope_ == Ope::WRITE) {
        obj[i].template allocate<YCSB>();
        // Materialize the payload before it gets std::move'd into update.
        [[maybe_unused]] YCSB& t = obj[i].ref();
#if TRACE
        t.id_ = izanagi_trace::next_gate_stamp(tx.thid_);
        izanagi_steps.push_back(
            {'W', izanagi_trace::key_to_hex(std::string(key[i].view())), "-",
             std::to_string(t.id_)});
#endif
#line 132
        tx.update(Storage::YCSB, key[i].view(),
                  TupleBody(key[i].view(), std::move(obj[i])));
      } else if (pro.ope_ == Ope::READ_MODIFY_WRITE) {
        TupleBody* body;
        tx.read(Storage::YCSB, key[i].view(), &body);
        if (tx.status_ != TransactionStatus::aborted) {
          YCSB& old_tuple = body->get_value().cast_to<YCSB>();
          obj[i].template allocate<YCSB>();
          YCSB& new_tuple = obj[i].ref();
          memcpy(new_tuple.val_, old_tuple.val_, VAL_SIZE);
#if TRACE
          const std::uint64_t izanagi_observed = old_tuple.id_;
          new_tuple.id_ = izanagi_trace::next_gate_stamp(tx.thid_);
          izanagi_steps.push_back(
              {'M', izanagi_trace::key_to_hex(std::string(key[i].view())),
               std::to_string(izanagi_observed),
               std::to_string(new_tuple.id_)});
#endif
#line 142
          tx.update(Storage::YCSB, key[i].view(),
                    TupleBody(key[i].view(), std::move(obj[i])));
        }
      } else {
        ERR;
      }

      if (tx.status_ == TransactionStatus::aborted) {
        tx.abort();
        ++tx.result_->local_abort_counts_;
#if ADD_ANALYSIS
        ++tx.result_->local_early_aborts_;
#endif
        goto RETRY;
      }

      i++;
    }

    if (!tx.commit()) {
      tx.abort();
      ++tx.result_->local_abort_counts_;
      goto RETRY;
    }
#if TRACE
    if (izanagi_trace::silo_ycsb_gate_enabled())
      izanagi_trace::emit_steps(tx.thid_, izanagi_steps);
#endif
#line 166
    storeRelease(tx.result_->local_commit_counts_,
                 loadAcquire(tx.result_->local_commit_counts_) + 1);

    return;
  }

  template <typename Tuple, typename Param>
  static void partTableInit([[maybe_unused]] size_t thid, Param* p,
                            uint64_t start, uint64_t end) {
#if MASSTREE_USE
    MasstreeWrapper<Tuple>::thread_init(thid);
#endif

    for (auto i = start; i <= end; ++i) {
      SimpleKey<8> key;
      YCSB::CreateKey(i, key.ptr());
      HeapObject obj;
      obj.allocate<YCSB>();
      YCSB& ycsb_tuple = obj.ref();
      ycsb_tuple.id_ = i;
      Tuple* tmp = new Tuple();
      tmp->init(thid, TupleBody(key.view(), std::move(obj)), p);
      Masstrees[get_storage(Storage::YCSB)].insert_value(key.view(), tmp);
    }
  }

  static uint32_t getTableNum() { return (uint32_t) Storage::Size; }

  template <typename Tuple, typename Param>
  static void makeDB(Param* p) {
    size_t maxthread = decideParallelBuildNumber(FLAGS_ycsb_tuple_num);

    std::vector<std::thread> thv;
    for (size_t i = 0; i < maxthread; ++i)
      thv.emplace_back(partTableInit<Tuple, Param>, i, p,
                       i * (FLAGS_ycsb_tuple_num / maxthread),
                       (i + 1) * (FLAGS_ycsb_tuple_num / maxthread) - 1);
    for (auto& th : thv) th.join();
  }

  static void displayWorkloadParameter() {
    cout << "#FLAGS_ycsb_max_ope:\t" << FLAGS_ycsb_max_ope << endl;
    cout << "#FLAGS_ycsb_rmw:\t" << FLAGS_ycsb_rmw << endl;
    cout << "#FLAGS_ycsb_rratio:\t" << FLAGS_ycsb_rratio << endl;
    cout << "#FLAGS_ycsb_tuple_num:\t" << FLAGS_ycsb_tuple_num << endl;
    cout << "#FLAGS_ycsb_zipf_skew:\t" << FLAGS_ycsb_zipf_skew << endl;
  }
};
