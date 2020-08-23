//
// Created by thawk on 2020/08/12.
//

#define GLOBAL_VALUE_DEFINE
#include "common.hh"
#include "gtest/gtest.h"
#include "tpcc/tpcc_initializer.hpp"

namespace ccbench::testing {

using namespace ccbench;
using namespace TPCC::Initializer;

class tpcc_initializer_test : public ::testing::Test {
};

TEST_F(tpcc_initializer_test, db_insert_test) { // NOLINT
  std::string key{"key"};
  std::string val{"val"};
  db_insert(Storage::STOCK, key, val, alignof(std::string));
  void *ret_ptr = kohler_masstree::find_record(Storage::STOCK, key);
  ASSERT_NE(ret_ptr, nullptr);
  std::string_view ret_val_view = static_cast<Record *>(ret_ptr)->get_tuple().get_val();
  ASSERT_EQ(memcmp(val.data(), ret_val_view.data(), val.size()), 0);
  ASSERT_EQ(true, false);
}

} // namespace ccbench::testing