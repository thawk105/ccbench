
#define GLOBAL_VALUE_DEFINE

#include "../include/common.hh"
#include "../../include/backoff.hh"

#include "glog/logging.h"
#include "gtest/gtest.h"

namespace ccbench::testing {

class make_db_test : public ::testing::Test {
public:
    void SetUp() override {}

    void TearDown() override {}
};

TEST_F(make_db_test, simple) { // NOLINT
    ASSERT_EQ(true, true);
}

} // namespace ccbench::testing
