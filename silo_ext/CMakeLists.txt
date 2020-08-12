cmake_minimum_required(VERSION 3.10)

project(ccbench_silo_ext
        VERSION 0.0.1
        DESCRIPTION "silo_ext of ccbench"
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
        "epoch.cpp"
        "garbage_collection.cpp"
        "interface_delete.cpp"
        "interface_helper.cpp"
        "interface_scan.cpp"
        "interface_search.cpp"
        "interface_termination.cpp"
        "interface_update_insert.cpp"
        "log.cpp"
        "scheme.cpp"
        "session_info.cpp"
        "session_info_table.cpp"
        "tid.cpp"
        "index/masstree_beta/masstree_beta_wrapper.cpp"
        )

add_executable(silo_ext.exe ${SILO_SOURCES} silo_ext.cpp)
add_executable(unit_test.exe ${SILO_SOURCES} test/unit_test.cpp)

target_include_directories(silo_ext.exe
        PRIVATE ${PROJECT_SOURCE_DIR}
        PRIVATE ${PROJECT_SOURCE_DIR}/../third_party
        PRIVATE ${PROJECT_SOURCE_DIR}/include
        PRIVATE ${PROJECT_SOURCE_DIR}/index/masstree_beta/include
        )

target_include_directories(unit_test.exe
        PRIVATE ${PROJECT_SOURCE_DIR}
        PRIVATE ${PROJECT_SOURCE_DIR}/../third_party
        PRIVATE ${PROJECT_SOURCE_DIR}/../third_party/googletest/googletest/include
        PRIVATE ${PROJECT_SOURCE_DIR}/include
        PRIVATE ${PROJECT_SOURCE_DIR}/index/masstree_beta/include
        )

target_link_libraries(silo_ext.exe
        Boost::filesystem
        gflags::gflags
        ${PROJECT_SOURCE_DIR}/../third_party/mimalloc/out/release/libmimalloc.a
        ${PROJECT_SOURCE_DIR}/../third_party/masstree/libkohler_masstree_json.a
        Threads::Threads
        )

target_link_libraries(unit_test.exe
        Boost::filesystem
        gflags::gflags
        ${PROJECT_SOURCE_DIR}/../third_party/mimalloc/out/release/libmimalloc.a
        ${PROJECT_SOURCE_DIR}/../third_party/masstree/libkohler_masstree_json.a
        ${PROJECT_SOURCE_DIR}/../third_party/googletest/build/lib/libgtest.a
        ${PROJECT_SOURCE_DIR}/../third_party/googletest/build/lib/libgtest_main.a
        Threads::Threads
        )

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    add_definitions(-DCCBENCH_LINUX)
endif ()

cmake_host_system_information(RESULT cores QUERY NUMBER_OF_LOGICAL_CORES)
add_definitions(-DKVS_EPOCH_TIME=40)
add_definitions(-DKVS_NUMBER_OF_LOGICAL_CORES=${cores})
add_definitions(-DKVS_MAX_PARALLEL_THREADS=500)
add_definitions(-DKVS_MAX_KEY_LENGTH=1000)
add_definitions(-DKVS_LOG_GC_THRESHOLD=1)
add_definitions(-DPROJECT_ROOT=${PROJECT_SOURCE_DIR})

