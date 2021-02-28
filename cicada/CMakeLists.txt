cmake_minimum_required(VERSION 3.10)

project(ccbench_cicada
        VERSION 0.0.1
        DESCRIPTION "cicada of ccbench"
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

file(GLOB CICADA_SOURCES
        "../common/result.cc"
        "../common/util.cc"
        "cicada.cc"
        "result.cc"
        "transaction.cc"
        "util.cc"
        )

add_executable(cicada.exe ${CICADA_SOURCES})

target_link_libraries(cicada.exe
        Boost::filesystem
        gflags::gflags
        ${PROJECT_SOURCE_DIR}/../third_party/mimalloc/out/release/libmimalloc.a
        ${PROJECT_SOURCE_DIR}/../third_party/masstree/libkohler_masstree_json.a
        Threads::Threads
        )

target_include_directories(cicada.exe
        PRIVATE ${PROJECT_SOURCE_DIR}/../include
        PRIVATE ${PROJECT_SOURCE_DIR}/../third_party/spdlog/include
        )

if (DEFINED ADD_ANALYSIS)
    add_definitions(-DADD_ANALYSIS=${ADD_ANALYSIS})
else ()
    add_definitions(-DADD_ANALYSIS=0)
endif ()

if (DEFINED BACK_OFF)
    add_definitions(-DBACK_OFF=${BACK_OFF})
else ()
    add_definitions(-DBACK_OFF=0)
endif ()

if (DEFINED INLINE_VERSION_PROMOTION)
    add_definitions(-DINLINE_VERSION_PROMOTION=${INLINE_VERSION_PROMOTION})
else ()
    add_definitions(-DINLINE_VERSION_PROMOTION=1)
endif ()

if (DEFINED INLINE_VERSION_OPT)
    add_definitions(-DINLINE_VERSION_OPT=${INLINE_VERSION_OPT})
else ()
    add_definitions(-DINLINE_VERSION_OPT=1)
endif ()

if (DEFINED KEY_SIZE)
    add_definitions(-DKEY_SIZE=${KEY_SIZE})
else ()
    add_definitions(-DKEY_SIZE=8)
endif ()

if (DEFINED MASSTREE_USE)
    add_definitions(-DMASSTREE_USE=${MASSTREE_USE})
else ()
    add_definitions(-DMASSTREE_USE=1)
endif ()

if (DEFINED PARTITION_TABLE)
    add_definitions(-DPARTITION_TABLE=${PARTITION_TABLE})
else ()
    add_definitions(-DPARTITION_TABLE=0)
endif ()

if (DEFINED REUSE_VERSION)
    add_definitions(-DREUSE_VERSION=${REUSE_VERSION})
else ()
    add_definitions(-DREUSE_VERSION=1)
endif ()

if (DEFINED SINGLE_EXEC)
    add_definitions(-DSINGLE_EXEC=${CSINGLE_EXEC})
else ()
    add_definitions(-DSINGLE_EXEC=0)
endif ()

if (DEFINED VAL_SIZE)
    add_definitions(-DVAL_SIZE=${VAL_SIZE})
else ()
    add_definitions(-DVAL_SIZE=4)
endif ()

if (DEFINED WRITE_LATEST_ONLY)
    add_definitions(-DWRITE_LATEST_ONLY=${WRITE_LATEST_ONLY})
else ()
    add_definitions(-DWRITE_LATEST_ONLY=0)
endif ()

if (DEFINED WORKER1_INSERT_DELAY_RPHASE)
    add_definitions(-DWORKER1_INSERT_DELAY_RPHASE=${WORKER1_INSERT_DELAY_RPHASE})
else ()
    add_definitions(-DWORKER1_INSERT_DELAY_RPHASE=0)
endif ()
