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
        "../common/result.cc"
        "../common/util.cc"
        "result.cc"
        "silo_ext.cc"
        "transaction.cc"
        "util.cc"
        )

add_executable(silo_ext.exe ${SILO_SOURCES})

target_link_libraries(silo_ext.exe
        Boost::filesystem
        gflags::gflags
        ${PROJECT_SOURCE_DIR}/../third_party/mimalloc/out/release/libmimalloc.a
        ${PROJECT_SOURCE_DIR}/../third_party/masstree/libkohler_masstree_json.a
        Threads::Threads
        )

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    add_definitions(-DCCBENCH_LINUX)
endif ()

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

if (DEFINED NO_WAIT_LOCKING_IN_VALIDATION)
    add_definitions(-DNO_WAIT_LOCKING_IN_VALIDATION=${NO_WAIT_LOCKING_IN_VALIDATION})
else ()
    add_definitions(-DNO_WAIT_LOCKING_IN_VALIDATION=1)
endif ()

if (DEFINED NO_WAIT_OF_TICTOC)
    add_definitions(-DNO_WAIT_OF_TICTOC=${NO_WAIT_OF_TICTOC})
else ()
    add_definitions(-DNO_WAIT_OF_TICTOC=0)
endif ()

if (DEFINED SLEEP_READ_PHASE)
    add_definitions(-DSLEEP_READ_PHASE=${SLEEP_READ_PHASE})
else ()
    add_definitions(-DSLEEP_READ_PHASE=0)
endif ()

if (DEFINED WAL)
    add_definitions(-DWAL=${WAL})
else ()
    add_definitions(-DWAL=0)
endif ()
