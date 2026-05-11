include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CompileOptions)

include_directories(${THIRD_PARTY_DIR}/googletest/googletest/include)

file(GLOB YCSB_OZE_SOURCES
        "../common/result.cc"
        "../common/util.cc"
        "ycsb_oze.cc"
        "result.cc"
        "transaction.cc"
        "util.cc"
        "debug.cc"
        )

file(GLOB TPCC_OZE_SOURCES
        "../common/result.cc"
        "../common/util.cc"
        "tpcc_oze.cc"
        "result.cc"
        "transaction.cc"
        "util.cc"
        "debug.cc"
        )

file(GLOB BOMB_OZE_SOURCES
        "../common/result.cc"
        "../common/util.cc"
        "bomb_oze.cc"
        "result.cc"
        "transaction.cc"
        "util.cc"
        "debug.cc"
        )

add_executable(ycsb_oze.exe ${YCSB_OZE_SOURCES})
add_executable(tpcc_oze.exe ${TPCC_OZE_SOURCES})
add_executable(bomb_oze.exe ${BOMB_OZE_SOURCES})

target_link_libraries(ycsb_oze.exe
        Boost::filesystem
        gflags::gflags
        glog::glog
        ${THIRD_PARTY_DIR}/mimalloc/out/release/libmimalloc.a
        ${THIRD_PARTY_DIR}/masstree/libkohler_masstree_json.a
        Threads::Threads
        )

target_link_libraries(tpcc_oze.exe
        Boost::filesystem
        gflags::gflags
        glog::glog
        ${THIRD_PARTY_DIR}/mimalloc/out/release/libmimalloc.a
        ${THIRD_PARTY_DIR}/masstree/libkohler_masstree_json.a
        Threads::Threads
        )

target_link_libraries(bomb_oze.exe
        Boost::filesystem
        gflags::gflags
        glog::glog
        ${THIRD_PARTY_DIR}/mimalloc/out/release/libmimalloc.a
        ${THIRD_PARTY_DIR}/masstree/libkohler_masstree_json.a
        Threads::Threads
        )

if (DEFINED ADD_ANALYSIS)
    add_definitions(-DADD_ANALYSIS=${ADD_ANALYSIS})
else ()
    add_definitions(-DADD_ANALYSIS=0)
endif ()

if (DEFINED BACK_OFF)
    add_definitions(-DBACK_OFF=${BACK_OFF})
else ()
    add_definitions(-DBACK_OFF=1)
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

if (DEFINED INSERT_READ_DELAY_MS)
    add_definitions(-DINSERT_READ_DELAY_MS=${INSERT_READ_DELAY_MS})
else ()
    remove_definitions(-DINSERT_READ_DELAY_MS)
endif ()

if (DEFINED INSERT_BATCH_DELAY_MS)
    add_definitions(-DINSERT_BATCH_DELAY_MS=${INSERT_BATCH_DELAY_MS})
else ()
    remove_definitions(-DINSERT_BATCH_DELAY_MS)
endif ()

if (DEFINED MERGE_ON_READ)
    add_definitions(-DMERGE_ON_READ=${MERGE_ON_READ})
else ()
    add_definitions(-DMERGE_ON_READ=0)
endif ()

if (DEFINED DEBUG_MSG)
    add_definitions(-DDEBUG_MSG=${DEBUG_MSG})
else ()
    add_definitions(-DDEBUG_MSG=0)
endif ()
