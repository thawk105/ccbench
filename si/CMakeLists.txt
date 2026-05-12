include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CompileOptions)

file(GLOB YCSB_SI_SOURCES
        "ycsb_si.cc"
        "garbage_collection.cc"
        "result.cc"
        "transaction.cc"
        "util.cc"
        )

file(GLOB TPCC_SI_SOURCES
        "tpcc_si.cc"
        "garbage_collection.cc"
        "result.cc"
        "transaction.cc"
        "util.cc"
        )

file(GLOB BOMB_SI_SOURCES
        "bomb_si.cc"
        "garbage_collection.cc"
        "result.cc"
        "transaction.cc"
        "util.cc"
        )

file(GLOB SBOMB_SI_SOURCES
        "sbomb_si.cc"
        "garbage_collection.cc"
        "result.cc"
        "transaction.cc"
        "util.cc"
        )

add_executable(ycsb_si.exe ${YCSB_SI_SOURCES})
add_executable(tpcc_si.exe ${TPCC_SI_SOURCES})
add_executable(bomb_si.exe ${BOMB_SI_SOURCES})
add_executable(sbomb_si.exe ${SBOMB_SI_SOURCES})


foreach(t ycsb_si.exe tpcc_si.exe bomb_si.exe sbomb_si.exe)
    target_link_libraries(${t} PRIVATE
            ccbench_common
            ccbench::masstree
            ccbench::mimalloc
            )
endforeach()
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
