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
  HeapObject obj;
  obj.allocate(b.size());
  ::memcpy(obj.data(), &b[0], b.size());
  auto *record_ptr = new Record{Tuple(a, std::move(obj))}; // NOLINT
  /**
   * If Status::OK is returned, the ownership of the memory moves to kohler_masstree,
   * so there is no responsibility for releasing it. If not, you are responsible for doing a delete record_ptr.
   */
  ASSERT_EQ(Status::OK, kohler_masstree::insert_record(Storage::CUSTOMER, a, record_ptr));
  ASSERT_EQ(kohler_masstree::find_record(Storage::DISTRICT, a), nullptr);
  Record *got_rec_ptr{static_cast<Record *>(kohler_masstree::find_record(Storage::CUSTOMER, a))};
  ASSERT_NE(got_rec_ptr, nullptr);
  ASSERT_EQ(got_rec_ptr->get_tuple().get_key(), std::string_view(a));
  ASSERT_EQ(got_rec_ptr->get_tuple().get_val(), std::string_view(b));
}

TEST_F(unit_test, tx_session_test) { // NOLINT
  Token token{};
  ASSERT_EQ(enter(token), Status::OK);
  ASSERT_EQ(leave(token), Status::OK);
}

TEST_F(unit_test, tx_delete_test) { // NOLINT
  Token token{};
  std::string a{"a"};
  std::string b{"b"};
  ASSERT_EQ(enter(token), Status::OK);
  ASSERT_EQ(insert(token, Storage::CUSTOMER, a, b, static_cast<std::align_val_t>(alignof(std::string))), Status::OK);
  Tuple *ret_tuple_ptr;
  ASSERT_EQ(commit(token), Status::OK);
  ASSERT_EQ(delete_record(token, Storage::CUSTOMER, a), Status::OK);
  ASSERT_EQ(commit(token), Status::OK);
  ASSERT_EQ(search_key(token, Storage::CUSTOMER, a, &ret_tuple_ptr), Status::WARN_NOT_FOUND);
  ASSERT_EQ(commit(token), Status::OK);
  ASSERT_EQ(leave(token), Status::OK);
}

TEST_F(unit_test, tx_insert_test) { // NOLINT
  Token token{};
  std::string a{"a"};
  std::string b{"b"};
  ASSERT_EQ(enter(token), Status::OK);
  ASSERT_EQ(insert(token, Storage::CUSTOMER, a, b, static_cast<std::align_val_t>(alignof(std::string))), Status::OK);
  Tuple *ret_tuple_ptr;
  ASSERT_EQ(search_key(token, Storage::CUSTOMER, a, &ret_tuple_ptr), Status::WARN_READ_FROM_OWN_OPERATION);
  ASSERT_EQ(ret_tuple_ptr->get_val(), std::string_view(b));
  ASSERT_EQ(commit(token), Status::OK);
  ASSERT_EQ(search_key(token, Storage::CUSTOMER, a, &ret_tuple_ptr), Status::OK);
  ASSERT_EQ(ret_tuple_ptr->get_val(), std::string_view(b));
  ASSERT_EQ(commit(token), Status::OK);
  ASSERT_EQ(leave(token), Status::OK);
}

TEST_F(unit_test, tx_scan_test) { // NOLINT
  Token token{};
  std::string a{"a"};
  std::string b{"b"};
  std::string c{"c"};
  std::string v1{"v1"};
  std::string v2{"v2"};
  std::string v3{"v3"};
  ASSERT_EQ(enter(token), Status::OK);
  ASSERT_EQ(insert(token, Storage::CUSTOMER, a, v1, static_cast<std::align_val_t>(alignof(std::string))), Status::OK);
  ASSERT_EQ(insert(token, Storage::CUSTOMER, b, v2, static_cast<std::align_val_t>(alignof(std::string))), Status::OK);
  ASSERT_EQ(insert(token, Storage::CUSTOMER, c, v3, static_cast<std::align_val_t>(alignof(std::string))), Status::OK);
  ASSERT_EQ(commit(token), Status::OK);
  std::vector<const Tuple *> tup_vec;
  ASSERT_EQ(scan_key(token, Storage::CUSTOMER, "", false, "", false, tup_vec), Status::OK);
  ASSERT_EQ(tup_vec.size(), static_cast<std::size_t>(3));
  for (std::size_t i = 0; i < 3; ++i) {
    if (i == 0) {
      ASSERT_EQ(tup_vec.at(i)->get_val(), std::string_view(v1));
    } else if (i == 1) {
      ASSERT_EQ(tup_vec.at(i)->get_val(), std::string_view(v2));
    } else {
      ASSERT_EQ(tup_vec.at(i)->get_val(), std::string_view(v3));
    }
  }
  ASSERT_EQ(commit(token), Status::OK);
  ASSERT_EQ(leave(token), Status::OK);
}

TEST_F(unit_test, tx_search_key_test) { // NOLINT
  Token token{};
  std::string a{"a"};
  std::string b{"b"};
  ASSERT_EQ(enter(token), Status::OK);
  Tuple *ret_tuple_ptr;
  ASSERT_EQ(search_key(token, Storage::CUSTOMER, a, &ret_tuple_ptr), Status::WARN_NOT_FOUND);
  ASSERT_EQ(commit(token), Status::OK);
  ASSERT_EQ(insert(token, Storage::CUSTOMER, a, b, static_cast<std::align_val_t>(alignof(std::string))), Status::OK);
  ASSERT_EQ(commit(token), Status::OK);
  ASSERT_EQ(search_key(token, Storage::CUSTOMER, a, &ret_tuple_ptr), Status::OK);
  ASSERT_EQ(ret_tuple_ptr->get_val(), std::string_view(b));
  ASSERT_EQ(commit(token), Status::OK);
  ASSERT_EQ(leave(token), Status::OK);
}

TEST_F(unit_test, tx_upsert_test) { // NOLINT
  Token token{};
  std::string a{"a"};
  std::string b{"b"};
  std::string c{"c"};
  ASSERT_EQ(enter(token), Status::OK);
  ASSERT_EQ(upsert(token, Storage::CUSTOMER, a, b, static_cast<std::align_val_t>(alignof(std::string))), Status::OK);
  Tuple *ret_tuple_ptr;
  ASSERT_EQ(search_key(token, Storage::CUSTOMER, a, &ret_tuple_ptr), Status::WARN_READ_FROM_OWN_OPERATION);
  ASSERT_EQ(ret_tuple_ptr->get_val(), std::string_view(b));
  ASSERT_EQ(commit(token), Status::OK);
  ASSERT_EQ(search_key(token, Storage::CUSTOMER, a, &ret_tuple_ptr), Status::OK);
  ASSERT_EQ(ret_tuple_ptr->get_val(), std::string_view(b));
  ASSERT_EQ(commit(token), Status::OK);
  ASSERT_EQ(upsert(token, Storage::CUSTOMER, a, c, static_cast<std::align_val_t>(alignof(std::string))), Status::OK);
  ASSERT_EQ(commit(token), Status::OK);
  ASSERT_EQ(search_key(token, Storage::CUSTOMER, a, &ret_tuple_ptr), Status::OK);
  ASSERT_EQ(ret_tuple_ptr->get_val(), std::string_view(c));
  ASSERT_EQ(commit(token), Status::OK);
  ASSERT_EQ(leave(token), Status::OK);
}
} // namespace ccbench::testing
