//
// Created by thawk on 2020/08/12.
//

#include "gtest/gtest.h"
#include "tpcc/tpcc_util.hpp"

namespace ccbench::testing {

using namespace ccbench;
using namespace TPCC;

class tpcc_util_test : public ::testing::Test {
};

TEST_F(tpcc_util_test, random_number_test) { // NOLINT
  for (std::size_t i = 0; i < 100; ++i) {
    std::uint64_t a = random_64bits();
    std::uint64_t b = random_64bits();
    std::uint64_t max_num = std::max(a, b);
    std::uint64_t min_num = std::min(a, b);
    std::uint64_t c = random_int(min_num, max_num);
    ASSERT_EQ(min_num <= c, true);
    ASSERT_EQ(c <= max_num, true);
  }
}

} // namespace ccbench::testing
