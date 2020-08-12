//
// Created by thawk on 2020/08/12.
//

#include "gtest/gtest.h"
#include "interface.h"
#include "masstree_beta_wrapper.h"

namespace ccbench::testing {

using namespace ccbench;

class unit_test : public ::testing::Test {
public:
  void SetUp() override { init(); }

  void TearDown() override { fin(); }
};

TEST_F(unit_test, direct_db_access) { // NOLINT
  std::string a{"a"};
  std::string b{"b"};
  Record record{a, b};
  kohler_masstree::insert_record(Storage::CUSTOMER, a, &record);
  ASSERT_EQ(kohler_masstree::find_record(Storage::DISTRICT, a), nullptr);
  Record *got_rec_ptr{static_cast<Record*>(kohler_masstree::find_record(Storage::CUSTOMER, a))};
  ASSERT_NE(got_rec_ptr, nullptr);
  ASSERT_EQ(got_rec_ptr->get_tuple().get_key(), std::string_view(a));
}

} // namespace ccbench::testing