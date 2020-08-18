//
// Created by thawk on 2020/08/12.
//

#include "gtest/gtest.h"
#include "aligned_allocator.h"

namespace ccbench::testing {

using namespace ccbench;

class aligned_allocator_test : public ::testing::Test {};

TEST_F(aligned_allocator_test, storage_test) { // NOLINT
  ASSERT_EQ(true, true);
  /**
   * TODO
   * 1. std::string a = new std::string(?, ccbench::aligned_allocator<std::string, b>)
   * 2. ASSERT_EQ(alignof(a.data()), b)
   */
}

} // namespace ccbench::testing