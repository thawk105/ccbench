include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CompileOptions)

file(GLOB YCSB_MOCC_SOURCES
        "../common/result.cc"
        "../common/util.cc"
        "lock.cc"
        "ycsb_mocc.cc"
        "result.cc"
        "transaction.cc"
        "util.cc"
        )

file(GLOB TPCC_MOCC_SOURCES
        "../common/result.cc"
        "../common/util.cc"
        "lock.cc"
        "tpcc_mocc.cc"
        "result.cc"
        "transaction.cc"
        "util.cc"
        )

file(GLOB BOMB_MOCC_SOURCES
        "../common/result.cc"
        "../common/util.cc"
        "lock.cc"
        "bomb_mocc.cc"
        "result.cc"
        "transaction.cc"
        "util.cc"
        )

file(GLOB SBOMB_MOCC_SOURCES
        "../common/result.cc"
        "../common/util.cc"
        "lock.cc"
        "sbomb_mocc.cc"
        "result.cc"
        "transaction.cc"
        "util.cc"
        )

add_executable(ycsb_mocc.exe ${YCSB_MOCC_SOURCES})
add_executable(tpcc_mocc.exe ${TPCC_MOCC_SOURCES})
add_executable(bomb_mocc.exe ${BOMB_MOCC_SOURCES})
add_executable(sbomb_mocc.exe ${SBOMB_MOCC_SOURCES})

target_link_libraries(ycsb_mocc.exe
        Boost::filesystem
        gflags::gflags
        ${THIRD_PARTY_DIR}/mimalloc/out/release/libmimalloc.a
        ${THIRD_PARTY_DIR}/masstree/libkohler_masstree_json.a
        Threads::Threads
        )

target_link_libraries(tpcc_mocc.exe
        Boost::filesystem
        gflags::gflags
        ${THIRD_PARTY_DIR}/mimalloc/out/release/libmimalloc.a
        ${THIRD_PARTY_DIR}/masstree/libkohler_masstree_json.a
        Threads::Threads
        )

target_link_libraries(bomb_mocc.exe
        Boost::filesystem
        gflags::gflags
        ${THIRD_PARTY_DIR}/mimalloc/out/release/libmimalloc.a
        ${THIRD_PARTY_DIR}/masstree/libkohler_masstree_json.a
        Threads::Threads
        )

target_link_libraries(sbomb_mocc.exe
        Boost::filesystem
        gflags::gflags
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

if (DEFINED KEY_SIZE)
    add_definitions(-DKEY_SIZE=${KEY_SIZE})
else ()
    add_definitions(-DKEY_SIZE=8)
endif ()

if (DEFINED KEY_SORT)
    add_definitions(-DKEY_SORT=${KEY_SORT})
else ()
    add_definitions(-DKEY_SORT=0)
endif ()

if (DEFINED MASSTREE_USE)
    add_definitions(-DMASSTREE_USE=${MASSTREE_USE})
else ()
    add_definitions(-DMASSTREE_USE=1)
endif ()

add_definitions(-DRWLOCK)

if (DEFINED TEMPERATURE_RESET_OPT)
    add_definitions(-DTEMPERATURE_RESET_OPT=${TEMPERATURE_RESET_OPT})
else ()
    add_definitions(-DTEMPERATURE_RESET_OPT=1)
endif ()

if (DEFINED VAL_SIZE)
    add_definitions(-DVAL_SIZE=${VAL_SIZE})
else ()
    add_definitions(-DVAL_SIZE=4)
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
