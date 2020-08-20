# CMake generated Testfile for 
# Source directory: /home/river/tpcc/ccbench/tpcc_silo/test
# Build directory: /home/river/tpcc/ccbench/tpcc_silo/test
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(aligned_allocator_test.exe "/home/river/tpcc/ccbench/tpcc_silo/test/aligned_allocator_test.exe" "--gtest_output=xml:aligned_allocator_test.exe_gtest_result.xml")
add_test(scheme_global_test.exe "/home/river/tpcc/ccbench/tpcc_silo/test/scheme_global_test.exe" "--gtest_output=xml:scheme_global_test.exe_gtest_result.xml")
add_test(unit_test.exe "/home/river/tpcc/ccbench/tpcc_silo/test/unit_test.exe" "--gtest_output=xml:unit_test.exe_gtest_result.xml")
