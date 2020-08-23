//
// Created by thawk on 2020/08/12.
//

#include "gtest/gtest.h"
#include "scheme_global.h"

namespace ccbench::testing {

using namespace ccbench;

class scheme_global_test : public ::testing::Test {
};

TEST_F(scheme_global_test, storage_test) { // NOLINT
  ASSERT_EQ(static_cast<int>(Storage::CUSTOMER), 0);
  ASSERT_EQ(static_cast<int>(Storage::DISTRICT), 1);
  ASSERT_EQ(static_cast<int>(Storage::HISTORY), 2);
  ASSERT_EQ(static_cast<int>(Storage::ITEM), 3);
  ASSERT_EQ(static_cast<int>(Storage::NEWORDER), 4);
  ASSERT_EQ(static_cast<int>(Storage::ORDER), 5);
  ASSERT_EQ(static_cast<int>(Storage::ORDERLINE), 6);
  ASSERT_EQ(static_cast<int>(Storage::STOCK), 7);
  ASSERT_EQ(static_cast<int>(Storage::WAREHOUSE), 8);
  ASSERT_EQ(static_cast<int>(Storage::SECONDARY), 9);
}

} // namespace ccbench::testing