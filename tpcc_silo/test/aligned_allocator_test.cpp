//
// Created by thawk on 2020/08/12.
//

#include "gtest/gtest.h"
#include "aligned_allocator.h"

namespace ccbench::testing {

using namespace ccbench;

class aligned_allocator_test : public ::testing::Test {
};

TEST_F(aligned_allocator_test, alignment_test) { // NOLINT
  ASSERT_EQ(true, true);
  constexpr std::size_t align = 64;
  auto *str = new std::basic_string<char, std::char_traits<char>, aligned_allocator<std::string, align>>();
  std::cout << reinterpret_cast<uintptr_t>(reinterpret_cast<void *>(&str)) << std::endl;
  std::cout << reinterpret_cast<uintptr_t>(reinterpret_cast<void *>(&str)) % align << std::endl;
  std::cout << reinterpret_cast<uintptr_t>(reinterpret_cast<void *>(str->data())) << std::endl;
  std::cout << reinterpret_cast<uintptr_t>(reinterpret_cast<void *>(str->data())) % align << std::endl;
  delete str;
}

} // namespace ccbench::testing