include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CompileOptions)

file(GLOB SBOMB_D2PL_SOURCES
        "../common/result.cc"
        "../common/util.cc"
        "result.cc"
        "sbomb_d2pl.cc"
        "transaction.cc"
        "util.cc"
        )

file(GLOB DBOMB_D2PL_SOURCES
        "../common/result.cc"
        "../common/util.cc"
        "result.cc"
        "dbomb_d2pl.cc"
        "transaction.cc"
        "util.cc"
        )

add_executable(sbomb_d2pl.exe ${SBOMB_D2PL_SOURCES})
add_executable(bomb_d2pl.exe ${DBOMB_D2PL_SOURCES})

target_link_libraries(sbomb_d2pl.exe
        Boost::filesystem
        gflags::gflags
        ${THIRD_PARTY_DIR}/mimalloc/out/release/libmimalloc.a
        ${THIRD_PARTY_DIR}/masstree/libkohler_masstree_json.a
        Threads::Threads
        )

target_link_libraries(bomb_d2pl.exe
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

if (DEFINED VAL_SIZE)
    add_definitions(-DVAL_SIZE=${VAL_SIZE})
else ()
    add_definitions(-DVAL_SIZE=4)
endif ()
