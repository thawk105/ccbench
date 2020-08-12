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
  Record *record_ptr = new Record{a, b};
  /**
   * If Status::OK is returned, the ownership of the memory moves to kohler_masstree,
   * so there is no responsibility for releasing it. If not, you are responsible for doing a delete record_ptr.
   */
  ASSERT_EQ(Status::OK, kohler_masstree::insert_record(Storage::CUSTOMER, a, record_ptr));
  ASSERT_EQ(kohler_masstree::find_record(Storage::DISTRICT, a), nullptr);
  Record *got_rec_ptr{static_cast<Record *>(kohler_masstree::find_record(Storage::CUSTOMER, a))};
  ASSERT_NE(got_rec_ptr, nullptr);
  ASSERT_EQ(got_rec_ptr->get_tuple().get_key(), std::string_view(a));
}

TEST_F(unit_test, tx_session_test) { // NOLINT
  Token token{};
  ASSERT_EQ(enter(token), Status::OK);
  ASSERT_EQ(leave(token), Status::OK);
}

TEST_F(unit_test, tx_insert_test) { // NOLINT
  Token token{};
  std::string a{"a"};
  std::string b{"b"};
  ASSERT_EQ(enter(token), Status::OK);
  ASSERT_EQ(insert(token, Storage::CUSTOMER, a, b), Status::OK);
  Tuple *ret_tuple_ptr;
  ASSERT_EQ(search_key(token, Storage::CUSTOMER, a, &ret_tuple_ptr), Status::WARN_READ_FROM_OWN_OPERATION);
  ASSERT_EQ(ret_tuple_ptr->get_val(), std::string_view(b));
  ASSERT_EQ(commit(token), Status::OK);
  ASSERT_EQ(search_key(token, Storage::CUSTOMER, a, &ret_tuple_ptr), Status::OK);
  ASSERT_EQ(ret_tuple_ptr->get_val(), std::string_view(b));
  ASSERT_EQ(commit(token), Status::OK);
  ASSERT_EQ(leave(token), Status::OK);
}

} // namespace ccbench::testing