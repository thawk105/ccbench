#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#define GLOBAL_VALUE_DEFINE

#include "../include/atomic_tool.hh"
#include "../include/transaction.hh"
#include "../../../include/tpcc/tpcc_tx_neworder.hh"

#include <gtest/gtest.h>

namespace {

template <typename T>
TupleBody MakeBody(std::string_view key, const T& value) {
  HeapObject object;
  object.allocate<T>();
  object.cast_to<T>() = value;
  return TupleBody(key, std::move(object));
}

class SiloTransactionTest : public ::testing::Test {
protected:
  bool quit_ = false;
  Result results_[2];
  uint64_t_64byte epochs_[2];
  uint64_t_64byte commit_tids_[2];
  TxExecutor owner_{0, &results_[0], quit_};
  TxExecutor observer_{1, &results_[1], quit_};
  std::vector<std::pair<Storage, std::string>> keys_;

  void SetUp() override {
    TotalThreadNum = 2;
    ThLocalEpoch = epochs_;
    CTIDW = commit_tids_;
    storeRelease(GlobalEpoch.obj_, uint64_t{1});
    storeRelease(ReclamationEpoch, uint32_t{0});
    FLAGS_tpcc_interactive_ms = 0;
    Backoff::Backoff_.store(0);
    MasstreeWrapper<Tuple>::thread_init(0);
    owner_.begin();
    observer_.begin();
  }

  void TearDown() override {
    // No transaction or Tuple pointer remains in use after the test body.
    owner_.abort();
    observer_.abort();
    storeRelease(ReclamationEpoch, std::numeric_limits<uint32_t>::max());
    owner_.gc_records();
    observer_.gc_records();
    for (const auto& [storage, key] : keys_) {
      auto& tree = Masstrees[get_storage(storage)];
      Tuple* tuple = tree.get_value(key);
      if (tuple != nullptr) {
        EXPECT_EQ(Status::OK, tree.remove_value(key));
        delete tuple;
      }
    }
    ThLocalEpoch = nullptr;
    CTIDW = nullptr;
  }

  static std::string Key(uint64_t value) {
    const auto encoded = __builtin_bswap64(value);
    return std::string(reinterpret_cast<const char*>(&encoded),
                       sizeof(encoded));
  }

  template <typename T>
  Tuple* Seed(Storage storage, std::string_view key, const T& value) {
    auto tuple = std::make_unique<Tuple>();
    tuple->init(0, MakeBody(key, value), nullptr);
    const Status status =
        Masstrees[get_storage(storage)].insert_value(key, tuple.get());
    EXPECT_EQ(Status::OK, status);
    if (status != Status::OK) return nullptr;
    keys_.emplace_back(storage, key);
    return tuple.release();
  }

  Status Insert(TxExecutor& tx, Storage storage, std::string_view key,
                uint64_t value) {
    keys_.emplace_back(storage, key);
    return tx.insert(storage, key, MakeBody(key, value));
  }
};

TEST_F(SiloTransactionTest, PendingInsertPointReadAbortsWithoutWaiting) {
  const auto key = Key(10);
  ASSERT_EQ(Status::OK, Insert(owner_, Storage::Order, key, 42));
  TupleBody sentinel;
  TupleBody* body = &sentinel;

  EXPECT_EQ(Status::ERROR_CONCURRENT_WRITE_OR_DELETE,
            observer_.read(Storage::Order, key, &body));
  EXPECT_EQ(TransactionStatus::aborted, observer_.status_);
  EXPECT_EQ(&sentinel, body);
  EXPECT_TRUE(observer_.read_set_.empty());
}

TEST_F(SiloTransactionTest, LimitedScanOfPendingInsertAbortsWithoutWaiting) {
  const auto first = Key(10);
  const auto committed = Key(20);
  ASSERT_EQ(Status::OK, Insert(owner_, Storage::NewOrder, first, 10));
  ASSERT_NE(nullptr, Seed(Storage::NewOrder, committed, uint64_t{20}));
  std::vector<TupleBody*> result;

  // A pending first row must not be reported as an empty district or allow
  // Delivery to choose the later committed row when LIMIT is one.
  EXPECT_EQ(Status::ERROR_CONCURRENT_WRITE_OR_DELETE,
            observer_.scan(Storage::NewOrder, first, false, Key(30), true,
                           result, 1));
  EXPECT_EQ(TransactionStatus::aborted, observer_.status_);
  EXPECT_TRUE(result.empty());
}

TEST_F(SiloTransactionTest, AbortedInsertRemainsAliveUntilReclamationEpoch) {
  const auto key = Key(10);
  ASSERT_EQ(Status::OK, Insert(owner_, Storage::Order, key, 42));
  Tuple* retained = Masstrees[get_storage(Storage::Order)].get_value(key);
  ASSERT_NE(nullptr, retained);
  storeRelease(GlobalEpoch.obj_, uint64_t{4});

  owner_.abort();

  EXPECT_EQ(nullptr, Masstrees[get_storage(Storage::Order)].get_value(key));
  ASSERT_EQ(1U, owner_.gc_records_.size());
  Tidword retired;
  retired.obj_ = loadAcquire(retained->tidword_.obj_);
  EXPECT_TRUE(retired.absent);
  EXPECT_FALSE(retired.lock);
  EXPECT_EQ(4U, retired.epoch);
  EXPECT_EQ(42U, retained->body_.get_value().cast_to<uint64_t>());
  EXPECT_EQ(Status::WARN_NOT_FOUND,
            observer_.read_internal(Storage::Order, key, retained));

  storeRelease(ReclamationEpoch, uint32_t{3});
  owner_.gc_records();
  ASSERT_EQ(1U, owner_.gc_records_.size());
  storeRelease(ReclamationEpoch, uint32_t{4});
  owner_.gc_records();
  EXPECT_TRUE(owner_.gc_records_.empty());
}

TEST_F(SiloTransactionTest, IndexVersionConflictInsertIsOwnedAndRolledBack) {
  const auto anchor = Key(10);
  const auto concurrent = Key(20);
  const auto rejected = Key(30);
  ASSERT_NE(nullptr, Seed(Storage::Order, anchor, uint64_t{10}));
  std::vector<TupleBody*> result;
  ASSERT_EQ(Status::OK,
            owner_.scan(Storage::Order, anchor, false, Key(40), true, result));
  ASSERT_FALSE(owner_.node_map_.empty());

  // Another transaction changes the scanned leaf before owner's INSERT.
  ASSERT_EQ(Status::OK, Insert(observer_, Storage::Order, concurrent, 20));
  ASSERT_TRUE(observer_.commit());
  EXPECT_EQ(Status::ERROR_CONCURRENT_WRITE_OR_DELETE,
            Insert(owner_, Storage::Order, rejected, 30));
  EXPECT_EQ(TransactionStatus::aborted, owner_.status_);
  ASSERT_NE(nullptr, owner_.searchWriteSet(Storage::Order, rejected));
  Tuple* retained = Masstrees[get_storage(Storage::Order)].get_value(rejected);
  ASSERT_NE(nullptr, retained);

  owner_.abort();

  EXPECT_EQ(nullptr,
            Masstrees[get_storage(Storage::Order)].get_value(rejected));
  ASSERT_EQ(1U, owner_.gc_records_.size());
  EXPECT_FALSE(retained->tidword_.lock);
  EXPECT_TRUE(retained->tidword_.absent);
}

TEST_F(SiloTransactionTest, BeginPinsEpochUntilTransactionCompletes) {
  storeRelease(GlobalEpoch.obj_, uint64_t{3});
  owner_.begin();
  EXPECT_EQ(3U, loadAcquire(ThLocalEpoch[0].obj_));
  const auto key = Key(10);
  ASSERT_NE(nullptr, Seed(Storage::Order, key, uint64_t{10}));
  TupleBody* body = nullptr;
  ASSERT_EQ(Status::OK, owner_.read(Storage::Order, key, &body));

  storeRelease(GlobalEpoch.obj_, uint64_t{5});
  ASSERT_TRUE(owner_.validationPhase());
  EXPECT_EQ(3U, loadAcquire(ThLocalEpoch[0].obj_));
  owner_.writePhase();
  owner_.begin();
  EXPECT_EQ(5U, loadAcquire(ThLocalEpoch[0].obj_));
}

TEST_F(SiloTransactionTest, DeleteRetirementWaitsForRemovalEpoch) {
  const auto key = Key(10);
  Tuple* retained = Seed(Storage::NewOrder, key, uint64_t{10});
  ASSERT_NE(nullptr, retained);
  ASSERT_EQ(Status::OK, owner_.delete_record(Storage::NewOrder, key));
  ASSERT_TRUE(owner_.validationPhase());

  // The pinned transaction epoch precedes removal from the index. A reader
  // may still have acquired this pointer in the newer epoch before removal.
  storeRelease(GlobalEpoch.obj_, uint64_t{3});
  owner_.writePhase();
  EXPECT_TRUE(retained->tidword_.absent);
  ASSERT_EQ(1U, owner_.gc_records_.size());
  storeRelease(ReclamationEpoch, uint32_t{1});
  owner_.gc_records();
  ASSERT_EQ(1U, owner_.gc_records_.size());
  storeRelease(ReclamationEpoch, uint32_t{3});
  owner_.gc_records();
  EXPECT_TRUE(owner_.gc_records_.empty());
}

TEST_F(SiloTransactionTest, ReadsAndScansSeeOwnInsertBody) {
  const auto key = Key(10);
  ASSERT_EQ(Status::OK, Insert(owner_, Storage::Order, key, 42));
  TupleBody* body = nullptr;
  ASSERT_EQ(Status::OK, owner_.read(Storage::Order, key, &body));
  ASSERT_NE(nullptr, body);
  ASSERT_TRUE(body->get_value().is_compatible<uint64_t>());
  EXPECT_EQ(42U, body->get_value().cast_to<uint64_t>());

  std::vector<TupleBody*> result;
  ASSERT_EQ(Status::OK,
            owner_.scan(Storage::Order, key, false, Key(20), true, result));
  ASSERT_EQ(1U, result.size());
  EXPECT_EQ(body, result.front());
}

TEST_F(SiloTransactionTest, OwnUpdatedReadsOverrideSnapshotWithinStorage) {
  const auto key = Key(10);
  ASSERT_NE(nullptr, Seed(Storage::Warehouse, key, uint64_t{1}));
  ASSERT_NE(nullptr, Seed(Storage::District, key, uint64_t{2}));
  TupleBody* body = nullptr;
  ASSERT_EQ(Status::OK, owner_.read(Storage::Warehouse, key, &body));
  ASSERT_EQ(Status::OK, owner_.read(Storage::District, key, &body));
  ASSERT_EQ(Status::OK,
            owner_.update(Storage::District, key, MakeBody(key, uint64_t{3})));
  ASSERT_EQ(Status::OK, owner_.read(Storage::District, key, &body));
  EXPECT_EQ(3U, body->get_value().cast_to<uint64_t>());
  ASSERT_EQ(Status::OK, owner_.read(Storage::Warehouse, key, &body));
  EXPECT_EQ(1U, body->get_value().cast_to<uint64_t>());

  std::vector<TupleBody*> result;
  ASSERT_EQ(Status::OK,
            owner_.scan(Storage::District, key, false, Key(20), true, result));
  ASSERT_EQ(1U, result.size());
  EXPECT_EQ(3U, result.front()->get_value().cast_to<uint64_t>());
}

TEST_F(SiloTransactionTest, ScanReallocationKeepsResultsInKeyOrder) {
  const auto first = Key(10);
  const auto second = Key(20);
  ASSERT_NE(nullptr, Seed(Storage::Order, first, uint64_t{10}));
  ASSERT_NE(nullptr, Seed(Storage::Order, second, uint64_t{20}));
  TupleBody* body = nullptr;
  ASSERT_EQ(Status::OK, owner_.read(Storage::Order, first, &body));
  owner_.read_set_.shrink_to_fit();
  ASSERT_EQ(owner_.read_set_.size(), owner_.read_set_.capacity());

  std::vector<TupleBody*> result;
  ASSERT_EQ(Status::OK,
            owner_.scan(Storage::Order, first, false, Key(30), true, result));
  ASSERT_EQ(2U, result.size());
  auto* first_read = owner_.searchReadSet(Storage::Order, first);
  auto* second_read = owner_.searchReadSet(Storage::Order, second);
  ASSERT_NE(nullptr, first_read);
  ASSERT_NE(nullptr, second_read);
  // Check ownership before dereferencing: the old scan returns a freed
  // first-row pointer after appending the second row reallocates read_set_.
  ASSERT_EQ(&first_read->body_, result[0]);
  ASSERT_EQ(&second_read->body_, result[1]);
  EXPECT_EQ(first, result[0]->get_key());
  EXPECT_EQ(second, result[1]->get_key());
  EXPECT_EQ(10U, result[0]->get_value().cast_to<uint64_t>());
  EXPECT_EQ(20U, result[1]->get_value().cast_to<uint64_t>());
}

TEST_F(SiloTransactionTest, RepeatedStockUpdatesAccumulateQuantitiesAndCounts) {
  Stock initial{};
  initial.S_W_ID = 1;
  initial.S_I_ID = 1;
  initial.S_QUANTITY = 16;
  initial.S_YTD = 100;
  initial.S_ORDER_CNT = 10;
  initial.S_REMOTE_CNT = 1;
  std::strcpy(initial.S_DIST_01, "district-data");
  SimpleKey<8> stock_key;
  initial.createKey(stock_key.ptr());
  ASSERT_NE(nullptr, Seed(Storage::Stock, stock_key.view(), initial));

  Stock first{};
  Stock second{};
  ASSERT_TRUE((get_and_update_stock<TxExecutor, TransactionStatus>(
      owner_, 1, 1, 7, false, first)));
  ASSERT_TRUE((get_and_update_stock<TxExecutor, TransactionStatus>(
      owner_, 1, 1, 9, true, second)));
  EXPECT_EQ(100, first.S_QUANTITY);
  EXPECT_EQ(91, second.S_QUANTITY);
  EXPECT_EQ(116U, second.S_YTD);
  EXPECT_EQ(12U, second.S_ORDER_CNT);
  EXPECT_EQ(2U, second.S_REMOTE_CNT);
  EXPECT_STREQ("district-data", first.S_DIST_01);
  EXPECT_STREQ("district-data", second.S_DIST_01);
  ASSERT_EQ(1U, owner_.write_set_.size());
  ASSERT_TRUE(owner_.commit());

  TupleBody* body = nullptr;
  ASSERT_EQ(Status::OK,
            observer_.read(Storage::Stock, stock_key.view(), &body));
  const auto& stored = body->get_value().cast_to<Stock>();
  EXPECT_EQ(91, stored.S_QUANTITY);
  EXPECT_EQ(116U, stored.S_YTD);
  EXPECT_EQ(12U, stored.S_ORDER_CNT);
  EXPECT_EQ(2U, stored.S_REMOTE_CNT);
  EXPECT_STREQ("district-data", stored.S_DIST_01);
}

TEST_F(SiloTransactionTest, DeleteBeforeUpdateDoesNotMarkUpdatedRowAbsent) {
  const auto key = Key(10);
  ASSERT_NE(nullptr, Seed(Storage::NewOrder, key, uint64_t{10}));
  Tuple* order = Seed(Storage::Order, key, uint64_t{20});
  ASSERT_NE(nullptr, order);
  ASSERT_EQ(Status::OK, owner_.delete_record(Storage::NewOrder, key));
  ASSERT_EQ(Status::OK,
            owner_.update(Storage::Order, key, MakeBody(key, uint64_t{30})));
  ASSERT_TRUE(owner_.commit());

  EXPECT_FALSE(order->tidword_.absent);
  TupleBody* body = nullptr;
  ASSERT_EQ(Status::OK, observer_.read(Storage::Order, key, &body));
  EXPECT_EQ(30U, body->get_value().cast_to<uint64_t>());
  EXPECT_EQ(Status::WARN_NOT_FOUND,
            observer_.read(Storage::NewOrder, key, &body));
}

TEST_F(SiloTransactionTest, ConcurrentDeleteRetiresRecordOnlyOnce) {
  const auto key = Key(10);
  ASSERT_NE(nullptr, Seed(Storage::NewOrder, key, uint64_t{10}));
  ASSERT_EQ(Status::OK, owner_.delete_record(Storage::NewOrder, key));
  ASSERT_EQ(Status::OK, observer_.delete_record(Storage::NewOrder, key));
  ASSERT_TRUE(owner_.commit());

  EXPECT_FALSE(observer_.commit());
  EXPECT_EQ(TransactionStatus::aborted, observer_.status_);
  EXPECT_EQ(1U, owner_.gc_records_.size());
  EXPECT_TRUE(observer_.gc_records_.empty());
}

TEST_F(SiloTransactionTest, UpdatingRemovedRecordReleasesItsAcquiredLock) {
  const auto key = Key(10);
  Tuple* retained = Seed(Storage::Order, key, uint64_t{10});
  ASSERT_NE(nullptr, retained);
  ASSERT_EQ(Status::OK,
            observer_.update(Storage::Order, key, MakeBody(key, uint64_t{20})));
  ASSERT_EQ(Status::OK, owner_.delete_record(Storage::Order, key));
  ASSERT_TRUE(owner_.commit());

  EXPECT_FALSE(observer_.commit());
  EXPECT_EQ(TransactionStatus::aborted, observer_.status_);
  Tidword removed;
  removed.obj_ = loadAcquire(retained->tidword_.obj_);
  EXPECT_TRUE(removed.absent);
  EXPECT_FALSE(removed.lock);
}

TEST_F(SiloTransactionTest, QuitStopsReadWaitingOnCommittedRecordLock) {
  const auto key = Key(10);
  Tuple* tuple = Seed(Storage::Order, key, uint64_t{10});
  ASSERT_NE(nullptr, tuple);
  Tidword locked = tuple->tidword_;
  locked.lock = true;
  storeRelease(tuple->tidword_.obj_, locked.obj_);
  storeRelease(quit_, true);
  TupleBody* body = nullptr;

  EXPECT_EQ(Status::ERROR_PREEMPTIVE_ABORT,
            observer_.read(Storage::Order, key, &body));
  EXPECT_EQ(TransactionStatus::aborted, observer_.status_);
  EXPECT_EQ(nullptr, body);
  locked.lock = false;
  storeRelease(tuple->tidword_.obj_, locked.obj_);
}

} // namespace
