cmake_minimum_required(VERSION 3.10)

project(ccbench_silo
        VERSION 0.0.1
        DESCRIPTION "silo of ccbench"
        LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/../cmake")

option(ENABLE_SANITIZER "enable sanitizer on debug build" ON)
option(ENABLE_UB_SANITIZER "enable undefined behavior sanitizer on debug build" OFF)
option(ENABLE_COVERAGE "enable coverage on debug build" OFF)

find_package(Doxygen)
find_package(Threads REQUIRED)
find_package(gflags REQUIRED)
find_package(glog REQUIRED)
find_package(Boost
        COMPONENTS filesystem)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CompileOptions)

file(GLOB SILO_SOURCES
        "../common/result.cc"
        "../common/util.cc"
        "result.cc"
        "silo.cc"
        "transaction.cc"
        "util.cc"
        )

file(GLOB REPLAY_SOURCES
        "replayTest.cc"
        )

add_executable(silo.exe ${SILO_SOURCES})
add_executable(replay_test.exe ${REPLAY_SOURCES})

target_link_libraries(silo.exe
        Boost::filesystem
        gflags::gflags
        ${PROJECT_SOURCE_DIR}/../third_party/masstree/libkohler_masstree_json.a
        Threads::Threads
        )

add_definitions(-DADD_ANALYSIS=1)
add_definitions(-DBACK_OFF=0)
add_definitions(-DKEY_SIZE=8)
add_definitions(-DMASSTREE_USE=1)
add_definitions(-DNO_WAIT_LOCKING_IN_VALIDATION=1)
add_definitions(-DNO_WAIT_OF_TICTOC=0)
add_definitions(-DPARTITION_TABLE=0)
add_definitions(-DPROCEDURE_SORT=0)
add_definitions(-DSLEEP_READ_PHASE=0) # of tics
add_definitions(-DVAL_SIZE=4)
add_definitions(-DWAL=0)
