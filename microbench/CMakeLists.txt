# Instruction-cost microbenchmarks. Reached only when the top-level
# CMakeLists is configured with -DCCBENCH_BUILD_MICROBENCH=ON.
#
# Each bench is a single translation unit linked against ccbench_common.
# See cmake/MicrobenchHelpers.cmake for ccbench_add_microbench().
include(MicrobenchHelpers)

ccbench_add_microbench(rdtscBench    SOURCES rdtscBench.cc)
ccbench_add_microbench(pow_test      SOURCES pow_test.cc)
ccbench_add_microbench(xoroshiro     SOURCES xoroshiro.cc)
ccbench_add_microbench(fetch_add     SOURCES fetch_add.cc)
ccbench_add_microbench(zipf_dist_test SOURCES zipf_dist_test.cc)
ccbench_add_microbench(membench      SOURCES membench.cc)
