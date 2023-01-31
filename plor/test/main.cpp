#include <gtest/gtest.h>
#include <atomic>
#include <bitset>
#include <random>
#include "../../include/atomic_wrapper.hh"

using namespace std;

class Unit: public ::testing::Test{};


TEST_F(Unit, init){
   int a;
   store(a ,5);
   ASSERT_EQ(a, 5);
}


int main(int argc, char **argv){
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}