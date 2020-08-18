cmake_minimum_required(VERSION 3.10)

project(ccbench_tpcc_silo
        VERSION 0.0.1
        DESCRIPTION "tpcc_silo of ccbench"
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
        "../common/util.cc"
        "epoch.cpp"
        "garbage_collection.cpp"
        "interface/interface_delete.cpp"
        "interface/interface_helper.cpp"
        "interface/interface_scan.cpp"
        "interface/interface_search.cpp"
        "interface/interface_termination.cpp"
        "interface/interface_update_insert.cpp"
        "log.cpp"
        "scheme.cpp"
        "session_info.cpp"
        "session_info_table.cpp"
        "tid.cpp"
        "index/masstree_beta/masstree_beta_wrapper.cpp"
        )

add_executable(silo_ext.exe ${SILO_SOURCES} silo_ext.cpp result.cpp util.cpp tpcc/tpcc_query.cpp tpcc/neworder.cpp tpcc/payment.cpp ../common/util.cc ../common/result.cc)
add_executable(river.exe ${SILO_SOURCES} test/river.cpp)
add_executable(river-2.exe ${SILO_SOURCES} test/river-2.cpp)
add_executable(unit_test.exe ${SILO_SOURCES} test/unit_test.cpp)
add_executable(scheme_global_test.exe test/scheme_global_test.cpp)
add_executable(aligned_allocator_test.exe test/aligned_allocator_test.cpp)
add_executable(river_test.exe test/river_test.cpp)

set_compile_options(silo_ext.exe)
set_compile_options(river.exe)
set_compile_options(river-2.exe)
set_compile_options(unit_test.exe)
set_compile_options(aligned_allocator_test.exe)
set_compile_options(scheme_global_test.exe)
set_compile_options(river_test.exe)

target_include_directories(silo_ext.exe
        PRIVATE ${PROJECT_SOURCE_DIR}
        PRIVATE ${PROJECT_SOURCE_DIR}/../third_party
        PRIVATE ${PROJECT_SOURCE_DIR}/include
        PRIVATE ${PROJECT_SOURCE_DIR}/index/masstree_beta/include
        PRIVATE ${PROJECT_SOURCE_DIR}/interface
        PRIVATE ${PROJECT_SOURCE_DIR}/tpcc
        )

target_include_directories(river.exe
        PRIVATE ${PROJECT_SOURCE_DIR}
        PRIVATE ${PROJECT_SOURCE_DIR}/../third_party
        PRIVATE ${PROJECT_SOURCE_DIR}/include
        PRIVATE ${PROJECT_SOURCE_DIR}/index/masstree_beta/include
        PRIVATE ${PROJECT_SOURCE_DIR}/interface
        PRIVATE ${PROJECT_SOURCE_DIR}/tpcc
        )

target_include_directories(river-2.exe
        PRIVATE ${PROJECT_SOURCE_DIR}
        PRIVATE ${PROJECT_SOURCE_DIR}/../third_party
        PRIVATE ${PROJECT_SOURCE_DIR}/include
        PRIVATE ${PROJECT_SOURCE_DIR}/index/masstree_beta/include
        PRIVATE ${PROJECT_SOURCE_DIR}/interface
        PRIVATE ${PROJECT_SOURCE_DIR}/tpcc
)

target_include_directories(scheme_global_test.exe
        PRIVATE ${PROJECT_SOURCE_DIR}
        PRIVATE ${PROJECT_SOURCE_DIR}/../third_party/googletest/googletest/include
        PRIVATE ${PROJECT_SOURCE_DIR}/include
        )

target_include_directories(aligned_allocator_test.exe
        PRIVATE ${PROJECT_SOURCE_DIR}
        PRIVATE ${PROJECT_SOURCE_DIR}/../third_party/googletest/googletest/include
        PRIVATE ${PROJECT_SOURCE_DIR}/include
        )

target_include_directories(river_test.exe
        PRIVATE ${PROJECT_SOURCE_DIR}
        PRIVATE ${PROJECT_SOURCE_DIR}/../third_party
        PRIVATE ${PROJECT_SOURCE_DIR}/../third_party/googletest/googletest/include
        PRIVATE ${PROJECT_SOURCE_DIR}/include
        PRIVATE ${PROJECT_SOURCE_DIR}/index/masstree_beta/include
        PRIVATE ${PROJECT_SOURCE_DIR}/interface
        PRIVATE ${PROJECT_SOURCE_DIR}/tpcc
        )

target_include_directories(unit_test.exe
        PRIVATE ${PROJECT_SOURCE_DIR}
        PRIVATE ${PROJECT_SOURCE_DIR}/../third_party
        PRIVATE ${PROJECT_SOURCE_DIR}/../third_party/googletest/googletest/include
        PRIVATE ${PROJECT_SOURCE_DIR}/include
        PRIVATE ${PROJECT_SOURCE_DIR}/index/masstree_beta/include
        PRIVATE ${PROJECT_SOURCE_DIR}/interface
        PRIVATE ${PROJECT_SOURCE_DIR}/tpcc
        )

target_link_libraries(silo_ext.exe
        Boost::filesystem
        gflags::gflags
        ${PROJECT_SOURCE_DIR}/../third_party/mimalloc/out/release/libmimalloc.a
        ${PROJECT_SOURCE_DIR}/../third_party/masstree/libkohler_masstree_json.a
        Threads::Threads
        )

target_link_libraries(river.exe
        Boost::filesystem
        gflags::gflags
        ${PROJECT_SOURCE_DIR}/../third_party/mimalloc/out/release/libmimalloc.a
        ${PROJECT_SOURCE_DIR}/../third_party/masstree/libkohler_masstree_json.a
        Threads::Threads
        )

target_link_libraries(river-2.exe
        Boost::filesystem
        gflags::gflags
        ${PROJECT_SOURCE_DIR}/../third_party/mimalloc/out/release/libmimalloc.a
        ${PROJECT_SOURCE_DIR}/../third_party/masstree/libkohler_masstree_json.a
        Threads::Threads
)

target_link_libraries(scheme_global_test.exe
        gflags::gflags
        Threads::Threads
        ${PROJECT_SOURCE_DIR}/../third_party/googletest/build/lib/libgtest.a
        ${PROJECT_SOURCE_DIR}/../third_party/googletest/build/lib/libgtest_main.a
        )

target_link_libraries(aligned_allocator_test.exe
        gflags::gflags
        Threads::Threads
        ${PROJECT_SOURCE_DIR}/../third_party/googletest/build/lib/libgtest.a
        ${PROJECT_SOURCE_DIR}/../third_party/googletest/build/lib/libgtest_main.a
        )

target_link_libraries(river_test.exe
        Boost::filesystem
        gflags::gflags
        ${PROJECT_SOURCE_DIR}/../third_party/mimalloc/out/release/libmimalloc.a
        ${PROJECT_SOURCE_DIR}/../third_party/masstree/libkohler_masstree_json.a
        ${PROJECT_SOURCE_DIR}/../third_party/googletest/build/lib/libgtest.a
        ${PROJECT_SOURCE_DIR}/../third_party/googletest/build/lib/libgtest_main.a
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
