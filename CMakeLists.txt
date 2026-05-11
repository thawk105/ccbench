cmake_minimum_required(VERSION 3.10)

project(ccbench LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

set(THIRD_PARTY_DIR "${PROJECT_SOURCE_DIR}/third_party/")

option(ENABLE_SANITIZER "enable sanitizer on debug build" ON)
option(ENABLE_UB_SANITIZER "enable undefined behavior sanitizer on debug build" OFF)
option(ENABLE_COVERAGE "enable coverage on debug build" OFF)

find_package(Doxygen)
find_package(Threads REQUIRED)
find_package(gflags REQUIRED)
find_package(glog REQUIRED)
find_package(Boost COMPONENTS filesystem)

add_subdirectory(cicada)
add_subdirectory(d2pl)
add_subdirectory(ermia)
add_subdirectory(mocc)
add_subdirectory(mvto)
add_subdirectory(oze)
#add_subdirectory(si)
add_subdirectory(silo)
add_subdirectory(ss2pl)
add_subdirectory(tictoc)
#add_subdirectory(tpcc_silo)
