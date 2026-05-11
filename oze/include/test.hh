#pragma once

#include "common.hh"
#include "result.hh"
#include "transaction.hh"
#include "tuple.hh"
#include "util.hh"

#include "../../include/tuple_body.hh"

#define NumVirtualThreads 16
#define ClocksPerUsec 2100

enum class Storage : std::uint32_t {
  TestTable = 0,
};

struct Data {
  alignas(CACHE_LINE_SIZE)
  std::uint64_t id_;
  char val_[1];

  //Primary Key: key_
  //key size is 8 bytes.
  static void CreateKey(uint64_t id, char *out) {
    assign_as_bigendian(id, &out[0]);
  }

  void createKey(char *out) const { return CreateKey(id_, out); }

  [[nodiscard]] std::string_view view() const { return struct_str_view(*this); }
};

class TestCase : public ::testing::Test {
protected:
    virtual void SetUp() {
        TotalThreadNum = NumVirtualThreads;
        ThManagementTable = new ThreadManagementEntry *[TotalThreadNum];
        ScanHistory = (atomic<ScanEntry*>*)calloc(2, sizeof(atomic<ScanEntry*>));
        for (int i = 0; i < TotalThreadNum; ++i) {
            txmap.emplace(i, new TxExecutor(i, backoff_, &res_[i], std::ref(quit_)));
            ThManagementTable[i] = new ThreadManagementEntry(1, FLAGS_cc_mode);
        }
        for (uint64_t i = (uint64_t)'a'; i <= (uint64_t)'z'; ++i) {
            std::string s{(char)i};
            table.emplace(s, new Tuple());
        }
        TxID txid = TxID();
        txid.thid = 0;
        for (auto& [key, tuple] : table) {
            HeapObject obj;
            obj.allocate<Data>();
            tuple->init(0, TupleBody(key, std::move(obj)), nullptr);
            for (auto& [k, t] : table) {
                tuple->graph_.at(txid).writeSet_.emplace(t);
            }
            Masstrees[get_storage(Storage::TestTable)].insert_value(key, tuple);
        }
        GlobalEpoch.obj_ = 1;
    }

    virtual void TearDown() {
        txmap.clear();
        table.clear();
        Masstrees[get_storage(Storage::TestTable)].table_init();
    }

    void clear() {
        TearDown();
        SetUp();
    }

    // wrappers for old test iterface in mock
    void begin(std::string txid) {
        txmap[std::stoi(txid)]->begin();
    }

    bool read(std::string key, std::string txid) {
        auto tx = txmap[std::stoi(txid)];
        tx->read(Storage::TestTable, key, &dummy_);
        if (tx->status_ == TransactionStatus::aborted)
            return false;
        else
            return true;
    }

    bool scan(std::string left_key, bool l_exclusive,
              std::string right_key, bool r_exclusive, std::string txid) {
        auto tx = txmap[std::stoi(txid)];
        std::vector<TupleBody*> results;
        tx->scan(Storage::TestTable, left_key, l_exclusive, right_key, r_exclusive, results);
        if (tx->status_ == TransactionStatus::aborted)
            return false;
        else
            return true;
    }

    bool write(std::string key, std::string txid) {
        auto tx = txmap[std::stoi(txid)];
        HeapObject obj;
        obj.allocate<Data>();
        tx->write(Storage::TestTable, key, TupleBody(key, std::move(obj)));
        if (tx->status_ == TransactionStatus::aborted)
            return false;
        else
            return true;
    }

    Status insert(std::string key, std::string txid) {
        auto tx = txmap[std::stoi(txid)];
        HeapObject obj;
        obj.allocate<Data>();
        return tx->insert(Storage::TestTable, key, TupleBody(key, std::move(obj)));
    }

    bool commit(std::string txid) {
        TxExecutor* tx = txmap[std::stoi(txid)];
        if (!tx->commit()) {
            tx->abort();
            return false;
        }
        return true;
    }

    bool exists_in_read_set(std::string txid, std::string key) {
        TxExecutor* tx = txmap[std::stoi(txid)];
        if (tx->searchReadSet(Storage::TestTable, key))
            return true;
        else
            return false;
    }

    std::string get_read_version(std::string txid, std::string key) {
        TxExecutor* tx = txmap[std::stoi(txid)];
        TxID version = tx->get_read_version(table[key]);
        return std::to_string(version.thid);
    }

    void set_latest_version_state(std::string key, VersionStatus status) {
        Tuple* tuple = Masstrees[get_storage(Storage::TestTable)].get_value(key);
        Version* v = tuple->ldAcqLatest();
        v->status_ = status;
    }

    std::map<int,TxExecutor*> txmap;
    std::map<std::string,Tuple*> table;
    TupleBody* dummy_;
    Backoff backoff_ = Backoff(ClocksPerUsec);
    Result res_[NumVirtualThreads];
    bool quit_ = false;
};
