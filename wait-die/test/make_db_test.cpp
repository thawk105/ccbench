
#include <mutex>

#define GLOBAL_VALUE_DEFINE

#include "../../include/backoff.hh"
#include "../include/common.hh"
#include "../include/util.hh"

#include "glog/logging.h"
#include "gtest/gtest.h"

namespace ccbench::testing {

class make_db_test : public ::testing::Test {
public:
    static void call_once_f() {
        google::InitGoogleLogging("make_db_test_log");
        FLAGS_stderrthreshold = 0;
    }

    void SetUp() override { std::call_once(init_, call_once_f); }

    void TearDown() override {}

private:
    static inline std::once_flag init_; // NOLINT
};

TEST_F(make_db_test, simple) { // NOLINT
    makeDB();
    // verify effect makeDb
    for (std::uint64_t i = 0; i < FLAGS_tuple_num; ++i) {
        ASSERT_EQ(Table[i].val_[0], 'a');
        ASSERT_EQ(Table[i].val_[1], '\0');
        ASSERT_EQ(Table[i].lock_.counter.load(std::memory_order_acquire), 0);
    }
}

} // namespace ccbench::testing