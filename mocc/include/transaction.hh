#pragma once

#include <vector>

#include "../../include/backoff.hh"
#include "../../include/procedure.hh"
#include "../../include/result.hh"
#include "../../include/status.hh"
#include "../../include/string.hh"
#include "../../include/util.hh"
#include "common.hh"
#include "lock.hh"
#include "mocc_op_element.hh"
#include "scan_callback.hh"
#include "tuple.hh"

using namespace std;

enum class TransactionStatus : uint8_t {
  invalid,
  inflight,
  committed,
  aborted,
};

class TxScanCallback;

class TxExecutor {
public:
  vector <ReadElement<Tuple>> read_set_;
  vector <WriteElement<Tuple>> write_set_;
  vector <Procedure> pro_set_;
  std::deque<Tuple*> gc_records_;
  std::unordered_map<void*, uint64_t> node_map_;
#ifdef RWLOCK
  vector<LockElement<ReaderWriterLock>> RLL_;
  vector<LockElement<ReaderWriterLock>> CLL_;
#endif  // RWLOCK
#ifdef MQLOCK
  vector<LockElement<MQLock>> RLL_;
  vector<LockElement<MQLock>> CLL_;
#endif  // MQLOCK
  TransactionStatus status_;
  TxScanCallback callback_;

  int thid_;
  Tidword mrctid_;
  Tidword max_rset_;
  Tidword max_wset_;
  Xoroshiro128Plus rnd_;
  Result *result_;
  uint64_t epoch_timer_start, epoch_timer_stop;
  Backoff backoff_;
  const bool& quit_; // for thread termination control

  bool reconnoitering_ = false;
  bool is_batch_ = false;
  bool is_ronly_ = false;

  TxExecutor(int thid, Result *res, const bool &quit)
    : result_(res), thid_(thid), quit_(quit), backoff_(FLAGS_clocks_per_us),
      callback_(TxScanCallback(this)) {
    this->status_ = TransactionStatus::inflight;
    this->rnd_.init();
    max_rset_.obj_ = 0;
    max_wset_.obj_ = 0;
  }

  ReadElement<Tuple> *searchReadSet(Storage s, std::string_view key);

  WriteElement<Tuple> *searchWriteSet(Storage s, std::string_view key);

  template<typename T>
  T *searchRLL(Tuple* key);

  void removeFromCLL(Tuple* key);

  void begin();

  Status read(Storage s, std::string_view key, TupleBody** body);

  Status read_internal(Storage s, std::string_view key, Tuple* tuple);

  Status write(Storage s, std::string_view key, TupleBody&& body);

  Status scan(Storage s,
              std::string_view left_key, bool l_exclusive,
              std::string_view right_key, bool r_exclusive,
              std::vector<TupleBody *>&result);

  Status scan(Storage s,
              std::string_view left_key, bool l_exclusive,
              std::string_view right_key, bool r_exclusive,
              std::vector<TupleBody *>&result, int64_t limit);

  Status insert(Storage s, std::string_view key, TupleBody&& body);

  Status delete_record(Storage s, std::string_view key);

  void lock(Tuple *tuple, bool mode);

  void construct_RLL();  // invoked on abort;
  void unlockCLL();

  bool validation();

  bool commit();

  void abort();

  void writePhase();

  bool isLeader();

  void leaderWork();

  void reconnoiter_begin(); 

  void reconnoiter_end(); 

  void gc_records();

  void dispCLL();

  void dispRLL();

  void dispWS();

  Tuple *get_tuple(Tuple *table, uint64_t key) { return &table[key]; }
};
